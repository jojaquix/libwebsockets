/*
 * Captive portal detect for Secure Streams
 *
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2019 - 2020 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <private-lib-core.h>

typedef struct ss_cpd {
	struct lws_ss_handle 	*ss;
	void			*opaque_data;
	/* ... application specific state ... */

	lws_sorted_usec_list_t	sul;

	uint8_t			partway;
} ss_cpd_t;

/* secure streams payload interface */

static int
ss_cpd_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	return 0;
}

static int
ss_cpd_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf,
		   size_t *len, int *flags)
{
	return 1;
}

static int
ss_cpd_state(void *userobj, void *sh, lws_ss_constate_t state,
	     lws_ss_tx_ordinal_t ack)
{
	ss_cpd_t *m = (ss_cpd_t *)userobj;
	struct lws_context *cx = (struct lws_context *)m->opaque_data;

	lwsl_info("%s: %s, ord 0x%x\n", __func__, lws_ss_state_name(state),
		  (unsigned int)ack);

	switch (state) {
	case LWSSSCS_CREATING:
		lws_ss_request_tx(m->ss);
		break;
	case LWSSSCS_QOS_ACK_REMOTE:
		lws_system_cpd_set(cx, LWS_CPD_INTERNET_OK);
		break;

	case LWSSSCS_ALL_RETRIES_FAILED:
	case LWSSSCS_DISCONNECTED:
		/*
		 * First result reported sticks... if nothing else, this will
		 * cover the situation we didn't connect to anything
		 */
		lws_system_cpd_set(cx, LWS_CPD_NO_INTERNET);
		break;

	default:
		break;
	}

	return 0;
}

int
lws_ss_sys_cpd(struct lws_context *cx)
{
	lws_ss_info_t ssi;

	/* We're making an outgoing secure stream ourselves */

	memset(&ssi, 0, sizeof(ssi));
	ssi.handle_offset	    = offsetof(ss_cpd_t, ss);
	ssi.opaque_user_data_offset = offsetof(ss_cpd_t, opaque_data);
	ssi.rx			    = ss_cpd_rx;
	ssi.tx			    = ss_cpd_tx;
	ssi.state		    = ss_cpd_state;
	ssi.user_alloc		    = sizeof(ss_cpd_t);
	ssi.streamtype		    = "captive_portal_detect";

	if (lws_ss_create(cx, 0, &ssi, cx, NULL, NULL, NULL)) {
		lwsl_info("%s: Create stream failed (policy?)\n", __func__);

		return 1;
	}

	return 0;
}
