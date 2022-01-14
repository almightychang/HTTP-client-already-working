#include "tcp_restclient.h"

__IO uint32_t message_count=0;

/* ECHO protocol states */
enum tcpclient_states
{
    TS_NOT_CONNECTED = 0,
    TS_CONNECTED,
    TS_RECEIVED,
    TS_CLOSING
};

/* structure to be passed as argument to the tcp callbacks */
struct tcpclient {
    u8_t state; /* connection status */
    u8_t retries;
    struct tcp_pcb *pcb;          /* pointer on the current tcp_pcb */
    struct pbuf *p;            /* pointer on pbuf to be transmitted */
};

struct tcpclient *tsTx = 0;
struct tcp_pcb *pcbTx = 0;

/* Private function prototypes -----------------------------------------------*/
static err_t tcp_restclient_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void tcp_restclient_connection_close(struct tcp_pcb *tpcb, struct tcpclient * es);
static err_t tcp_restclient_poll(void *arg, struct tcp_pcb *tpcb);
static err_t tcp_restclient_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static void tcp_restclient_send(struct tcp_pcb *tpcb, struct tcpclient * es);
static err_t tcp_restclient_connected(void *arg, struct tcp_pcb *tpcb, err_t err);

err_t tcp_restclient_connect(void)
{
    ip_addr_t DestIPaddr;
    struct tcp_pcb *tcp_pcb = tcp_new();

    if( tcp_pcb != NULL )
    {
        IP4_ADDR( &DestIPaddr, DEST_IP_ADDR0, DEST_IP_ADDR1, DEST_IP_ADDR2, DEST_IP_ADDR3 );
        return tcp_connect(tcp_pcb, &DestIPaddr, DEST_PORT, tcp_restclient_connected);
    }
}

static err_t tcp_restclient_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    struct tcpclient *ts = NULL;

    /* close connection */
    if( err != ERR_OK ) tcp_restclient_connection_close(tpcb, ts);

    /* allocate structure ts to maintain tcp connection informations */
    ts = (struct tcpclient *)mem_malloc(sizeof(struct tcpclient));

    /* TODO: FOR DEBUGGING */
    u8_t   data[200];
    //sprintf((char*)data, "sending tcp client message %d", (int)message_count);
    sprintf((char*)data, HTTPC_REQ_11);
    ts->p = pbuf_alloc(PBUF_TRANSPORT, strlen((char*)data) , PBUF_POOL);
    pbuf_take(ts->p, (char*)data, strlen((char*)data));

    if( ts == NULL )
    {
        /* close connection */
        tcp_restclient_connection_close(tpcb, ts);

        /* return memory allocation error */
        return ERR_MEM;  
    }

    ts->state = TS_CONNECTED;
    ts->pcb = tpcb;

    /* pass newly allocated es structure as argument to tpcb */
    tcp_arg(tpcb, ts);

    /* initialize LwIP tcp_recv callback function */ 
    tcp_recv(tpcb, tcp_restclient_recv);

    /* initialize LwIP tcp_sent callback function */
    tcp_sent(tpcb, tcp_restclient_sent);

    /* initialize LwIP tcp_poll callback function */
    tcp_poll(tpcb, tcp_restclient_poll, 1);

    /* send data */
    tcp_restclient_send(tpcb, ts); //TODO: delte

    return ERR_OK;
}

static err_t tcp_restclient_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    struct tcpclient *ts;
    err_t ret_err;

    LWIP_ASSERT("arg != NULL",arg != NULL);

    ts = (struct tcpclient *)arg;

    /* if we receive an empty tcp frame from server => close connection */
    if (p == NULL)
    {
        /* remote host closed connection */
        ts->state = TS_CLOSING;
        if(ts->p == NULL)
        {
            /* we're done sending, close connection */
            tcp_restclient_connection_close(tpcb, ts);
        }
        else
        {    
            /* do nothing */
        }
        ret_err = ERR_OK;
    }   
    /* else : a non empty frame was received from echo server but for some reason err != ERR_OK */
    else if(err != ERR_OK)
    {
        /* free received pbuf*/
        if (p != NULL)
        {
            ts->p = NULL;
            pbuf_free(p);
        }
        ret_err = err;
    }
    else if(ts->state == TS_CONNECTED)
    {
        ts->p = p;

        /* Increment message count */
        message_count++;

        /* Acknowledge data reception */
        tcp_recved(tpcb, p->tot_len);  

        /* Handle received data */
        // tcp_restclient_handle(tpcb, ts);

        /* store Tx framework */
	    tsTx = ts; //TODO
	    pcbTx = tpcb; //TODO

        pbuf_free(p);

        ret_err = ERR_OK;
    }
    /* data received when connection already closed */
    else
    {
        /* Acknowledge data reception */
        tcp_recved(tpcb, p->tot_len);

        /* free pbuf and do nothing */
        pbuf_free(p);
        ret_err = ERR_OK;
    }
    return ret_err;
}

static void tcp_restclient_connection_close(struct tcp_pcb *tpcb, struct tcpclient * ts)
{
    /* remove all callbacks */
    tcp_arg(tpcb, NULL);
    tcp_sent(tpcb, NULL);
    tcp_recv(tpcb, NULL);
    tcp_err(tpcb, NULL);
    tcp_poll(tpcb, NULL, 0);

    /* delete es structure */
    if (ts != NULL)
    {
        mem_free(ts);
    }

    /* close tcp connection */
    tcp_close(tpcb);
}

static err_t tcp_restclient_poll(void *arg, struct tcp_pcb *tpcb)
{
    err_t ret_err;
    struct tcpclient *ts;

    ts = (struct tcpclient *)arg;
    if (ts != NULL)
    {
        if (ts->p != NULL)
        {
            // tcp_sent(tpcb, tcp_restclient_sent);
            /* there is a remaining pbuf (chain) , try to send data */
            // tcp_restclient_send(tpcb, es);
        }
        else
        {
            /* no remaining pbuf (chain)  */
            if(ts->state == TS_CLOSING)
            {
                /*  close tcp connection */
                tcp_restclient_connection_close(tpcb, ts);
            }
        }
        ret_err = ERR_OK;
    }
    else
    {
        /* nothing to be done */
        tcp_abort(tpcb);
        ret_err = ERR_ABRT;
    }
    return ret_err;
}
static err_t tcp_restclient_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct tcpclient *ts;

    LWIP_UNUSED_ARG(len);

    ts = (struct tcpclient *)arg;
    ts->retries = 0;

    if(ts->p != NULL)
    {
        /* still got pbufs to send */
        //tcp_sent(tpcb, tcp_echoserver_sent);
        //tcp_restclient_send(tpcb, es);
    }
    else
    {
        /* if no more data to send and client closed connection*/
        if(ts->state == TS_CLOSING)
        tcp_restclient_connection_close(tpcb, ts);
    }
    return ERR_OK;
}
static void tcp_restclient_send(struct tcp_pcb *tpcb, struct tcpclient * ts)
{
    struct pbuf *ptr;
    err_t wr_err = ERR_OK;

    while ((wr_err == ERR_OK) &&
    (ts->p != NULL) && 
    (ts->p->len <= tcp_sndbuf(tpcb)))
    {

        /* get pointer on pbuf from ts structure */
        ptr = ts->p;

        /* enqueue data for transmission */
        wr_err = tcp_write(tpcb, ptr->payload, ptr->len, 1);

        if (wr_err == ERR_OK)
        {
            u16_t plen;
            u8_t freed;

            plen = ptr->len;

            /* continue with next pbuf in chain (if any) */
            ts->p = ptr->next;

            if(ts->p != NULL)
            {
                /* increment reference count for es->p */
                pbuf_ref(ts->p);
            }

            /* chop first pbuf from chain */
            do
            {
                /* try hard to free pbuf */
                freed = pbuf_free(ptr);
            }
            while(freed == 0);
            /* we can read more data now */
            //tcp_recved(tpcb, plen);
        }
        else if(wr_err == ERR_MEM)
        {
            /* we are low on memory, try later / harder, defer to poll */
            ts->p = ptr;
        }
        else
        {
            /* other problem ?? */
        }
    }
}


void test(void)
{
    char buf[100];

    int len = sprintf (buf, "Sending TCPclient Message\n");

        /* allocate pbuf */
        tsTx->p = pbuf_alloc(PBUF_TRANSPORT, len , PBUF_POOL);


        /* copy data to pbuf */
        pbuf_take(tsTx->p, (char*)buf, len);

        tcp_restclient_send(pcbTx, tsTx);

        pbuf_free(tsTx->p);
}
