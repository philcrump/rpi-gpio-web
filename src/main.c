/*
 * lws-minimal-http-server-form-post
 *
 * Copyright (C) 2018 Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal http server that performs POST with a couple
 * of parameters.  It dumps the parameters to the console log and redirects
 * to another page.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>

#include "gpio.h"

/*
 * Unlike ws, http is a stateless protocol.  This pss only exists for the
 * duration of a single http transaction.  With http/1.1 keep-alive and http/2,
 * that is unrelated to (shorter than) the lifetime of the network connection.
 */
struct pss {
    struct lws_spa *spa;
};

static int interrupted;

static const char * const param_names[] = {
    "state",
};

enum enum_param_names {
    EPN_STATE
};

bool gpio_state;

static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
          void *in, size_t len)
{
    struct pss *pss = (struct pss *)user;
    uint8_t buf[LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE], *start = &buf[LWS_PRE],
        *p = start, *end = &buf[sizeof(buf) - 1];
    int n;

    switch (reason) {
    case LWS_CALLBACK_HTTP:

        /*
         * Manually report that our form target URL exists
         *
         * you can also do this by adding a mount for the form URL
         * to the protocol with type LWSMPRO_CALLBACK, then no need
         * to trap LWS_CALLBACK_HTTP.
         */

        if (0 == strcmp((const char *)in, "/hpa_power_set"))
        {
            if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
                "application/json",
                LWS_ILLEGAL_HTTP_CONTENT_LEN, /* no content len */
                &p, end))
                return 1;
            if (lws_finalize_write_http_header(wsi, start, &p, end))
                return 1;

            /* write the body separately */
            lws_callback_on_writable(wsi);

            /* assertively allow it to exist in the URL space */
            return 0;
        }        

        /* default to 404-ing the URL if not mounted */
        break;

    case LWS_CALLBACK_HTTP_BODY:

        /* create the POST argument parser if not already existing */

        if (!pss->spa) {
            pss->spa = lws_spa_create(wsi, param_names,
                    LWS_ARRAY_SIZE(param_names), 1024,
                    NULL, NULL); /* no file upload */
            if (!pss->spa)
                return -1;
        }

        /* let it parse the POST data */

        if (lws_spa_process(pss->spa, in, (int)len))
            return -1;
        break;

    case LWS_CALLBACK_HTTP_BODY_COMPLETION:

        /* inform the spa no more payload data coming */

        lws_spa_finalize(pss->spa);

        /* we just dump the decoded things to the log */

        for (n = 0; n < (int)LWS_ARRAY_SIZE(param_names); n++) {
            if (!lws_spa_get_string(pss->spa, n))
                lwsl_user("%s: undefined\n", param_names[n]);
            else
                lwsl_user("%s: (len %d) '%s'\n",
                    param_names[n],
                    lws_spa_get_length(pss->spa, n),
                    lws_spa_get_string(pss->spa, n));
        }

        /* Filter for state */
        if(0 == strcmp((const char *)lws_spa_get_string(pss->spa, EPN_STATE), "on"))
        {
            GPIO4_set(true);
            gpio_state = true;
        }
        else if(0 == strcmp((const char *)lws_spa_get_string(pss->spa, EPN_STATE), "off"))
        {
            GPIO4_set(false);
            gpio_state = false;
        }

        return 0;

        break;

    case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
        /* called when our wsi user_space is going to be destroyed */
        if (pss->spa) {
            lws_spa_destroy(pss->spa);
            pss->spa = NULL;
        }
        break;

    case LWS_CALLBACK_HTTP_WRITEABLE:

        if (!pss)
            break;

        n = LWS_WRITE_HTTP_FINAL;

        if(gpio_state == true)
        {
            p += lws_snprintf((char *)p, end - p, "{\"state\": true}");
        }
        else
        {
            p += lws_snprintf((char *)p, end - p, "{\"state\": false}");
        }

        if (lws_write(wsi, (uint8_t *)start, lws_ptr_diff(p, start), n) !=
                lws_ptr_diff(p, start))
            return 1;

        /*
         * HTTP/1.0 no keepalive: close network connection
         * HTTP/1.1 or HTTP1.0 + KA: wait / process next transaction
         * HTTP/2: stream ended, parent connection remains up
         */

        if (lws_http_transaction_completed(wsi))
        {
            return -1;
        }

        return 0;

    default:
        break;
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static struct lws_protocols protocols[] = {
    {
        .name = "http",
        .callback = callback_http,
        .per_session_data_size = sizeof(struct pss),
        .rx_buffer_size = 0,
    },
    {
        0 /* terminator */
    }
};

/* default mount serves the URL space from ./mount-origin */

static const struct lws_http_mount mount = {
    /* .mount_next */          NULL,        /* linked-list "next" */
    /* .mountpoint */       "/",        /* mountpoint URL */
    /* .origin */       "./htdocs",   /* serve from dir */
    /* .def */          "index.html",   /* default filename */
    /* .protocol */         NULL,
    /* .cgienv */           NULL,
    /* .extra_mimetypes */      NULL,
    /* .interpret */        NULL,
    /* .cgi_timeout */      0,
    /* .cache_max_age */        0,
    /* .auth_mask */        0,
    /* .cache_reusable */       0,
    /* .cache_revalidate */     0,
    /* .cache_intermediaries */ 0,
    /* .origin_protocol */      LWSMPRO_FILE,   /* files in a dir */
    /* .mountpoint_len */       1,      /* char count */
    /* .basic_auth_login_file */    NULL,
    /* __dummy */ { 0 },
};

void sigint_handler(int sig)
{
    (void)sig;
    interrupted = 1;
}

int main(int argc, const char **argv)
{
    (void)argc;
    (void)argv;

    struct lws_context_creation_info info;
    struct lws_context *context;
    int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN; // | LLL_NOTICE
            /* for LLL_ verbosity above NOTICE to be built into lws,
             * lws must have been configured and built with
             * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
            /* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
            /* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
            /* | LLL_DEBUG */;

    signal(SIGINT, sigint_handler);

    GPIO4_setup();
    GPIO4_set(false);
    gpio_state = false;

    lws_set_log_level(logs, NULL);

    memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
    info.port = 80;
    info.protocols = protocols;
    info.mounts = &mount;
    info.options =
        LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

    context = lws_create_context(&info);
    if (!context) {
        lwsl_err("lws init failed\n");
        return 1;
    }

    while (n >= 0 && !interrupted)
        n = lws_service(context, 1000);

    lws_context_destroy(context);

    GPIO4_set(false);
    gpio_state = false;

    return 0;
}