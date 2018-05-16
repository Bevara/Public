#include "fitz.h"

void fz_rebind_stream(fz_stream *stm, fz_context *ctx)
{
	if (stm == NULL || stm->ctx == ctx)
		return;
	do {
		stm->ctx = ctx;
		stm = (stm->rebind == NULL ? NULL : stm->rebind(stm));
	} while (stm != NULL);
}

fz_stream *
fz_new_stream(fz_context *ctx, void *state,
	fz_stream_next_fn *next,
	fz_stream_close_fn *close,
	fz_stream_rebind_fn *rebind)
{
	fz_stream *stm;


	

	fz_try(ctx)
	{
		stm = fz_malloc_struct(ctx, fz_stream);
	}
	fz_catch(ctx)
	{
		close(ctx, state);
		fz_rethrow(ctx);
	}

	stm->refs = 1;
	stm->error = 0;
	stm->eof = 0;
	stm->pos = 0;

	stm->bits = 0;
	stm->avail = 0;

	stm->rp = NULL;
	stm->wp = NULL;

	stm->state = state;
	stm->next = next;
	stm->close = close;
	stm->seek = NULL;
	stm->rebind = rebind;
	stm->ctx = ctx;

	return stm;
}

fz_stream *
fz_keep_stream(fz_stream *stm)
{
	if (stm)
		stm->refs ++;
	return stm;
}

void
fz_close(fz_stream *stm)
{
	if (!stm)
		return;
	stm->refs --;
	if (stm->refs == 0)
	{
		if (stm->close)
			stm->close(stm->ctx, stm->state);
		fz_free(stm->ctx, stm);
	}
}

/* File stream */

typedef struct fz_file_stream_s
{
  /* BEVARA changed over to point to inbuf */
	/* int file; */
  char *file;
  unsigned char buffer[4096];
} fz_file_stream;

int next_file(fz_stream *stm, int n)
{
	fz_file_stream *state = stm->state;
	int temp;



	/* n is only a hint, that we can safely ignore */
	/* n = read(state->file, state->buffer, sizeof(state->buffer)); */
	/* if (n < 0) */
	/* 	fz_throw(stm->ctx, FZ_ERROR_GENERIC, "read error: "); */

	/* changed to memcpy into the buffer */
	if (stm->INBUF_BYTES_LEFT == 0)
	  return EOF;

	temp = sizeof(state->buffer);
	if (temp>stm->INBUF_BYTES_LEFT)
	  temp = stm->INBUF_BYTES_LEFT;
	  
	memcpy(state->buffer,(state->file)+stm->INBUF_BYTES_START,temp);
	stm->INBUF_BYTES_START += temp;
	stm->INBUF_BYTES_LEFT -= temp;
	stm->rp = state->buffer;
	stm->wp = state->buffer + temp;
	stm->pos += temp; /* INBUF_BYTES_START and pos should be equal */


	/* stm->rp = state->buffer; */
	/* stm->wp = state->buffer + n; */
	/* stm->pos += n; */
	/* if (n == 0) */
	/* 	return EOF; */
	return *stm->rp++;
}

static void seek_file(fz_stream *stm, int offset, int whence)
{
	fz_file_stream *state = stm->state;
	/* three cases for whence */
	if (whence == SEEK_SET)
		{
			/*printf("SEEK_SET\n");*/
			/* break up for debugging */
			if (offset >= stm->INBUF_BYTES_START)
			       stm->INBUF_BYTES_LEFT -= (offset-stm->INBUF_BYTES_START);
			else
				stm->INBUF_BYTES_LEFT += (stm->INBUF_BYTES_START-offset);
			stm->INBUF_BYTES_START = offset;

		}
	else if (whence == SEEK_CUR)
		{
			/*printf("SEEK_CUR\n");*/
			stm->INBUF_BYTES_LEFT -=  offset;
			stm->INBUF_BYTES_START += offset;

		}
	else if (whence == SEEK_END)
		{
			/*printf("SEEK_END\n");*/
			stm->INBUF_BYTES_START = stm->INBUF_BYTES_START+stm->INBUF_BYTES_LEFT-offset;
			stm->INBUF_BYTES_LEFT=offset;
		}
	else {
	  	fz_throw(stm->ctx, FZ_ERROR_GENERIC, "cannot seek - invalid whence ");
	}

	/* int n = lseek(state->file, offset, whence); */
	/* if (n < 0) */
	/* 	fz_throw(stm->ctx, FZ_ERROR_GENERIC, "cannot lseek: "); */
	stm->pos = stm->INBUF_BYTES_START;
	stm->rp = state->buffer;
	stm->wp = state->buffer;
}

static void close_file(fz_context *ctx, void *state_)
{
	/* fz_file_stream *state = state_; */
	/* int n = close(state->file); */
	/* if (n < 0) */
	/* 	fz_warn(ctx, "close error:"); */

  /* BEVARA: changed to handle changed fz_file_stream struct, caller 
   frees up the inbuf */
  /* fz_free(ctx, state); */
  fz_free(ctx,NULL);
}

fz_stream *
fz_open_inbuf(fz_context *ctx, char *inbuf, int insize)
{
	fz_stream *stm;
	fz_file_stream *state = fz_malloc_struct(ctx, fz_file_stream);
	state->file = inbuf;

	
	fz_try(ctx)
	{
		stm = fz_new_stream(ctx, state, next_file, close_file, NULL);
		/* added new field to keep track of total size */
		stm->INPUT_SIZE = insize;
		stm->INBUF_BYTES_START = 0; /* start at begining of buffer */
		stm->INBUF_BYTES_LEFT = insize; /* init have full range of buffer available */
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state);
		fz_rethrow(ctx);
	}
	stm->seek = seek_file;

	return stm;
}




/* Memory stream */

static int next_buffer(fz_stream *stm, int max)
{
  // printf("\t\t make it to next buffer\n");
	return EOF;
}

static void seek_buffer(fz_stream *stm, int offset, int whence)
{

  // printf("\t\t made it to seek buffer\n");
	int pos = stm->pos - (stm->wp - stm->rp);
	/* Convert to absolute pos */
	if (whence == 1)
	{
		offset += pos; /* Was relative to current pos */
	}
	else if (whence == 2)
	{
		offset += stm->pos; /* Was relative to end */
	}

	if (offset < 0)
		offset = 0;
	if (offset > stm->pos)
		offset = stm->pos;
	stm->rp += offset - pos;
}

static void close_buffer(fz_context *ctx, void *state_)
{
	fz_buffer *state = (fz_buffer *)state_;
	if (state)
		fz_drop_buffer(ctx, state);
}

fz_stream *
fz_open_buffer(fz_context *ctx, fz_buffer *buf)
{



	fz_stream *stm;

	fz_keep_buffer(ctx, buf);
	stm = fz_new_stream(ctx, buf, next_buffer, close_buffer, NULL);


	stm->INBUF_BYTES_START = 0; /* start at begining of buffer */
	stm->INBUF_BYTES_LEFT = buf->len; /* init have full range of buffer available */
	stm->seek = seek_buffer;

	stm->rp = buf->data;
	stm->wp = buf->data + buf->len;

	stm->pos = buf->len;


	return stm;
}

fz_stream *
fz_open_memory(fz_context *ctx, unsigned char *data, int len)
{
	fz_stream *stm;

	stm = fz_new_stream(ctx, NULL, next_buffer, close_buffer, NULL);
	stm->seek = seek_buffer;

	stm->rp = data;
	stm->wp = data + len;

	stm->pos = len;

	return stm;
}
