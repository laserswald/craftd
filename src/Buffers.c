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

#include <craftd/Buffers.h>

CDBuffers*
CD_CreateBuffers (void)
{
    CDBuffers* self = CD_malloc(sizeof(CDBuffers));

    if (!self) {
        return NULL;
    }

    self->input  = CD_CreateBuffer();
    self->output = CD_CreateBuffer();

    self->raw      = NULL;
    self->external = false;

    return self;
}

CDBuffers*
CD_WrapBuffers (CDRawBuffers buffers)
{
     CDBuffers* self = CD_malloc(sizeof(CDBuffers));

    if (!self) {
        return NULL;
    }

    self->input  = CD_WrapBuffer(bufferevent_get_input(buffers));
    self->output = CD_WrapBuffer(bufferevent_get_output(buffers));

    self->raw      = buffers;
    self->external = true;

    return self;
}

void
CD_DestroyBuffers (CDBuffers* self)
{
    CD_DestroyBuffer(self->input);
    CD_DestroyBuffer(self->output);

    CD_free(self);
}
