/*
 * Copyright (c) 2010-2011 Kevin M. Bowling, <kevin.bowling@kev009.com>, USA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Worker.h"
#include "common.h"

CDWorker*
CD_CreateWorker (void)
{
    CDWorker* self = CD_malloc(sizeof(CDWorker));

    if (!object) {
        return NULL;
    }

    self->id      = 0;
    self->working = NULL;
    self->thread  = NULL;

    return object;
}

void
CD_DestroyWorker (CDWorker* self)
{
    if (self->thread) {
        self->working = false;
        pthread_cancel(self->thread);
    }

    CD_free(self->working);
    CD_free(self);
}

void*
CD_RunWorker (void* arg)
{
    CDWorker* self = arg;

    LOG(LOG_INFO, "worker %d started", id);

    while (self->working) {
        pthread_mutex_lock(&self->workers->mutex);

        /* Check our predicate again:  The WQ is not empty
         * Prevent a nasty race condition if the client disconnects
         * Works in tandem with errorcb FOREACH bev removal loop
         */
        while (CD_ListLength(self->workers->jobs) == 0) {
            LOG(LOG_DEBUG, "worker %d ready", self->id);

            pthread_cond_wait(&self->workers->condition, &self->workers->mutex);
        }

        if (!self->working) {
            LOG(LOG_DEBUG, "worker %d stopped", self->id);
            pthread_mutex_unlock(self->workers->mutex);
            break;
        }

        LOG(LOG_DEBUG, "in worker: %d", self->id);

        do {
            self->job = CD_NextJob(self->workers);

            if (CD_JOB_IS_PLAYER(self->job)) {
                CDPlayer* player = self->job->data;

                pthread_rwlock_rdlock(&player->lock.status);

                switch (self->job->type) {
                    case CDPlayerInputJob: {
                        if (player->status != CDPlayerIdle) {
                            CD_AddJob(self->workers, self->job);
                            self->job = NULL;
                        }
                    } break;

                    case CDPlayerProcessJob: {
                        if (player->status != CDPlayerInput) {
                            CD_AddJob(self->workers, self->job);
                            self->job = NULL;
                        }
                    } break;

                    case CDPlayerOutputJob: {

                    } break;
                }

                pthread_rwlock_unlock(&player->lock.status);
            }
        } while (!self->job);

        pthread_mutex_unlock(self->workers->mutex);

        if (CD_JOB_IS_PLAYER(self->job)) {
            CDPlayer* player = self->job->data;

            pthread_rwlock_wrlock(&player->lock.pending);
            pthread_rwlock_wrlock(&player->lock.status);

            switch (self->job->type) {
                case CDPlayerInput: {
                    if (!self->job->event || !self->job->player) {
                        LOG(LOG_CRIT, "Aaack, null event or context in worker?");

                        goto PLAYER_JOB_ERROR;
                    }

                    CD_HashSet(PRIVATE(player), "packet", CD_PacketFromEvent(player->event));

                    if (evbuffer_get_length(player->event->input) > 0) {
                        player->pending = true;

                        CD_AddJob(player->server->workers, self->job);
                    }
                    else {
                        player->pending = false;

                        CD_DestroyJob(self->job);
                    }

                    player->status = CDPlayerInput;
                } break;

                case CDPlayerProcess: {
                    player->status = CDPlayerProcess;
                    pthread_rwlock_unlock(&player->lock.status);
                    CD_EventDispatch(player->server, "Player.process", player);
                    pthread_rwlock_wrlock(&player->lock.status);
                    player->status = CDPlayerIdle;
                } break;

                default: {
                    CD_DestroyJob(self->job);
                }
            }

            PLAYER_JOB_DONE: {
                self->job = NULL;

                pthread_rwlock_unlock(&player->lock.pending);
                pthread_rwlock_unlock(&player->lock.status);
            }

            PLAYER_JOB_ERROR: {
                self->job = NULL;

                pthread_rwlock_unlock(&player->lock.pending);
                pthread_rwlock_unlock(&player->lock.status);
            }
        }
        else {
            CD_DestroyJob(self->job);
            self->job = NULL;
        }
    }

    return NULL;
}
