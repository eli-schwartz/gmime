/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2007 Jeffrey Stedfast
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gmime-stream-null.h"

static void g_mime_stream_null_class_init (GMimeStreamNullClass *klass);
static void g_mime_stream_null_init (GMimeStreamNull *stream, GMimeStreamNullClass *klass);
static void g_mime_stream_null_finalize (GObject *object);

static ssize_t stream_read (GMimeStream *stream, char *buf, size_t len);
static ssize_t stream_write (GMimeStream *stream, const char *buf, size_t len);
static int stream_flush (GMimeStream *stream);
static int stream_close (GMimeStream *stream);
static gboolean stream_eos (GMimeStream *stream);
static int stream_reset (GMimeStream *stream);
static off_t stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence);
static off_t stream_tell (GMimeStream *stream);
static ssize_t stream_length (GMimeStream *stream);
static GMimeStream *stream_substream (GMimeStream *stream, off_t start, off_t end);


static GMimeStreamClass *parent_class = NULL;


GType
g_mime_stream_null_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeStreamNullClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_stream_null_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeStreamNull),
			16,   /* n_preallocs */
			(GInstanceInitFunc) g_mime_stream_null_init,
		};
		
		type = g_type_register_static (GMIME_TYPE_STREAM, "GMimeStreamNull", &info, 0);
	}
	
	return type;
}


static void
g_mime_stream_null_class_init (GMimeStreamNullClass *klass)
{
	GMimeStreamClass *stream_class = GMIME_STREAM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GMIME_TYPE_STREAM);
	
	object_class->finalize = g_mime_stream_null_finalize;
	
	stream_class->read = stream_read;
	stream_class->write = stream_write;
	stream_class->flush = stream_flush;
	stream_class->close = stream_close;
	stream_class->eos = stream_eos;
	stream_class->reset = stream_reset;
	stream_class->seek = stream_seek;
	stream_class->tell = stream_tell;
	stream_class->length = stream_length;
	stream_class->substream = stream_substream;
}

static void
g_mime_stream_null_init (GMimeStreamNull *stream, GMimeStreamNullClass *klass)
{
	stream->written = 0;
	stream->newlines = 0;
}

static void
g_mime_stream_null_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static ssize_t
stream_read (GMimeStream *stream, char *buf, size_t len)
{
	memset (buf, 0, len);
	
	stream->position += len;
	
	return len;
}

static ssize_t
stream_write (GMimeStream *stream, const char *buf, size_t len)
{
	GMimeStreamNull *null = (GMimeStreamNull *) stream;
	register const char *inptr = buf;
	const char *inend = buf + len;
	
	while (inptr < inend) {
		if (*inptr == '\n')
			null->newlines++;
		inptr++;
	}
	
	null->written += len;
	stream->position += len;
	
	return len;
}

static int
stream_flush (GMimeStream *stream)
{
	return 0;
}

static int
stream_close (GMimeStream *stream)
{
	return 0;
}

static gboolean
stream_eos (GMimeStream *stream)
{
	return TRUE;
}

static int
stream_reset (GMimeStream *stream)
{
	GMimeStreamNull *null = (GMimeStreamNull *) stream;
	
	null->written = 0;
	null->newlines = 0;
	
	return 0;
}

static off_t
stream_seek (GMimeStream *stream, off_t offset, GMimeSeekWhence whence)
{
	GMimeStreamNull *null = (GMimeStreamNull *) stream;
	off_t bound_end;
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : null->written;
	
	switch (whence) {
	case GMIME_STREAM_SEEK_SET:
		stream->position = MIN (offset + stream->bound_start, bound_end);
		break;
	case GMIME_STREAM_SEEK_END:
		stream->position = MAX (offset + bound_end, 0);
		break;
	case GMIME_STREAM_SEEK_CUR:
		stream->position += offset;
		if (stream->position < stream->bound_start)
			stream->position = stream->bound_start;
		else if (stream->position > bound_end)
			stream->position = bound_end;
	}
	
	return stream->position;
}

static off_t
stream_tell (GMimeStream *stream)
{
	return stream->position;
}

static ssize_t
stream_length (GMimeStream *stream)
{
	GMimeStreamNull *null = GMIME_STREAM_NULL (stream);
	off_t bound_end;
	
	bound_end = stream->bound_end != -1 ? stream->bound_end : null->written;
	
	return bound_end - stream->bound_start;
}

static GMimeStream *
stream_substream (GMimeStream *stream, off_t start, off_t end)
{
	GMimeStreamNull *null;
	
	null = g_object_new (GMIME_TYPE_STREAM_NULL, NULL);
	
	g_mime_stream_construct (GMIME_STREAM (null), start, end);
	
	return GMIME_STREAM (null);
}


/**
 * g_mime_stream_null_new:
 *
 * Creates a new GMimeStreamNull object.
 *
 * Returns a new null stream (similar to /dev/null on Unix).
 **/
GMimeStream *
g_mime_stream_null_new (void)
{
	GMimeStreamNull *null;
	
	null = g_object_new (GMIME_TYPE_STREAM_NULL, NULL);
	
	g_mime_stream_construct (GMIME_STREAM (null), 0, -1);
	
	return GMIME_STREAM (null);
}
