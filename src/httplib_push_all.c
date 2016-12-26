/* 
 * Copyright (c) 2016 Lammert Bies
 * Copyright (c) 2013-2016 the Civetweb developers
 * Copyright (c) 2004-2013 Sergey Lyubka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * ============
 * Release: 2.0
 */

#include "httplib_main.h"
#include "httplib_ssl.h"
#include "httplib_utils.h"

/*
 * static int push( struct httplib_context *ctx, FILE *fp, SOCKET sock, SSL *ssl, const char *buf, int len, double timeout );
 *
 * The function push() writes data to the I/O chanel, opened file descriptor,
 * socket or SSL descriptor. The function returns the number of bytes which
 * were actually written.
 */

static int push( struct httplib_context *ctx, FILE *fp, SOCKET sock, SSL *ssl, const char *buf, int len, double timeout ) {

	struct timespec start;
	struct timespec now;
	int n;
	int err;

#ifdef _WIN32
	typedef int len_t;
#else
	typedef size_t len_t;
#endif

	if ( ctx == NULL ) return -1;
#ifdef NO_SSL
	if ( ssl != NULL ) return -1;
#endif

	if ( timeout > 0 ) {

		memset( & start, 0, sizeof(start) );
		memset( & now,   0, sizeof(now)   );

		clock_gettime( CLOCK_MONOTONIC, &start );
	}

	do {

#ifndef NO_SSL
		if ( ssl != NULL ) {

			n = SSL_write( ssl, buf, len );

			if ( n <= 0 ) {

				err = SSL_get_error( ssl, n );

				if      ( err == SSL_ERROR_SYSCALL   &&  n   == -1                   ) err = ERRNO;
				else if ( err == SSL_ERROR_WANT_READ ||  err == SSL_ERROR_WANT_WRITE ) n   = 0;
				else return -1;

			}
			
			else err = 0;
		}
		
		else
#endif
		if ( fp != NULL ) {

			n = (int)fwrite( buf, 1, (size_t)len, fp );
			if ( ferror(fp) ) {

				n   = -1;
				err = ERRNO;
			}
			
			else err = 0;
		}
		
		else {
			n = (int)send( sock, buf, (len_t)len, MSG_NOSIGNAL );
			err = ( n < 0 ) ? ERRNO : 0;

			if ( n == 0 ) {

				/*
				 * shutdown of the socket at client side
				 */

				return -1;
			}
		}

		if ( ctx->stop_flag ) return -1;

		if ( n > 0  ||  (n == 0 && len == 0) ) {

			/*
			 * some data has been read, or no data was requested
			 */

			return n;
		}

		if ( n < 0 ) {

			/*
			 * socket error - check errno
			 *
			 * TODO: error handling depending on the error code.
			 * These codes are different between Windows and Linux.
			 */

			return -1;
		}

		/*
		 * This code is not reached in the moment.
		 * ==> Fix the TODOs above first.
		 */

		if ( timeout > 0 ) clock_gettime( CLOCK_MONOTONIC, &now );

	} while ( timeout <= 0  ||  XX_httplib_difftimespec( &now, &start ) <= timeout );

	return -1;

}  /* push */



/*
 * int64_t XX_httplib_push_all( struct httplib_context *ctx, FILE *fp, SOCKET sock, SSL *ssl, const char *buf, int64_t len );
 *
 * The function XX_httplib_push_all() pushes all data in a buffer to a socket.
 * The number of bytes written is returned.
 */

int64_t XX_httplib_push_all( struct httplib_context *ctx, FILE *fp, SOCKET sock, SSL *ssl, const char *buf, int64_t len ) {

	double timeout;
	int64_t n;
	int64_t nwritten;

	if ( ctx == NULL ) return -1;

	nwritten = 0;

	if ( ctx->cfg[REQUEST_TIMEOUT] != NULL ) timeout = atoi( ctx->cfg[REQUEST_TIMEOUT] ) / 1000.0;
	else                                     timeout = -1.0;

	while ( len > 0  &&  ctx->stop_flag == 0 ) {

		n = push( ctx, fp, sock, ssl, buf + nwritten, (int)len, timeout );

		if ( n < 0 ) {

			/*
			 * Propagate the error
			 */

			if ( nwritten == 0 ) nwritten = n;
			break;
		}
		
		else if ( n == 0 ) {

			/*
			 * No more data to write
			 */

			break;
		}
		
		else {
			nwritten += n;
			len      -= n;
		}
	}

	return nwritten;

}  /* XX_httplib_push_all */
