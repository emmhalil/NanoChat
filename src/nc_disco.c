#include "nc.h"

static void
handle_request(int sock, struct sockaddr_in sock_remote_addr,
	       char *body, char *remote_addr, int remote_port,
	       nc_opts *opts);

void*
nc_disco_loop(void *opts)
{
  int sock;
  
  struct sockaddr_in sock_local_addr;
  struct sockaddr_in sock_remote_addr;
  unsigned short sock_local_port;
  unsigned short sock_remote_port;
  
  socklen_t sock_remote_len = sizeof(sock_remote_addr);

  char recv_str[DCMD_LEN + 1];
  int recv_str_len;

  sock_local_port = atoi(DISCO_PORT);
  memset(&sock_local_addr, 0, sizeof(sock_local_addr));
  sock_local_addr.sin_family = AF_INET;
  sock_local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  sock_local_addr.sin_port = htons(sock_local_port);

  if((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    nc_utils_die("nc:disco:loop:sock");

  if(bind(sock, (struct sockaddr *) &sock_local_addr, sizeof(sock_local_addr)) < 0)
    nc_utils_die("nc:disco:loop:bind");

  nc_log_writef("info", "Disco was started on port %s.", DISCO_PORT);

  for(;;) {
    
    if((recv_str_len = recvfrom(sock, recv_str, DCMD_LEN, 0,
				(struct sockaddr *) &sock_remote_addr,
				&sock_remote_len)) < 0)
      nc_utils_die("nc:disco:loop:recvfrom");

    sock_remote_port = ntohs(sock_remote_addr.sin_port);
    recv_str[recv_str_len] = '\0';

    nc_log_writef("info", "Discovery from %s:%d got: %s",
		  inet_ntoa(sock_remote_addr.sin_addr),
		  sock_remote_port,
		  recv_str);

    handle_request(sock, sock_remote_addr, recv_str,
		   inet_ntoa(sock_remote_addr.sin_addr),
		   sock_remote_port,
		   opts);
  }
}

int
nc_disco_probe(nc_opts *opts)
{
  return nc_udp_send(opts->broadcast, DISCO_PORT,
		     DCMD_PROBE_REQUEST_CODE, 1);
}

void
nc_disco_start(nc_opts *opts)
{
  pthread_t disco_loop;
  pthread_create(&disco_loop, NULL, nc_disco_loop, opts);
}

void
handle_request(int sock, struct sockaddr_in sock_remote_addr,
	       char *body, char *remote_addr, int remote_port,
	       nc_opts *opts)
{
  char *body_payload;

  int response_len = DCMD_CODE_LEN + PORT_MAX;
  char response[response_len];
  
  if(strncmp(body, DCMD_PROBE_REQUEST_CODE, DCMD_CODE_LEN) == 0) {

    /* request */
    /* send RPC port as response */
    body_payload = body + DCMD_CODE_LEN;
    strcpy(response, DCMD_PROBE_RESPONSE_CODE);
    strcat(response, opts->port);
    nc_log_writef("info", "Incoming discovery (UDP) request. code: %s, body: %s",
		  DCMD_PROBE_REQUEST_CODE, body_payload);

    /* reply response to DISCO_PORT */
    nc_udp_send(inet_ntoa(sock_remote_addr.sin_addr), DISCO_PORT,
		DCMD_PROBE_RESPONSE_CODE, 0);
    
    /* reply ack to sock */
    if(sendto(sock, DCMD_PROBE_ACK_CODE, DCMD_CODE_LEN, 0,
	      (struct sockaddr *) &sock_remote_addr,
	      sizeof(sock_remote_addr)) == -1)
      nc_utils_die("nc:disco:handler_request:sendto");
    
  } else if(strncmp(body, DCMD_PROBE_RESPONSE_CODE, DCMD_CODE_LEN) == 0) {

    /* response */
    /* receive RPC port as response */
    body_payload = body + DCMD_CODE_LEN;
    nc_log_writef("info", "Incoming discovery (UDP) response. code: %s, body: %s",
		  DCMD_PROBE_RESPONSE_CODE, body_payload);

    /* @TODO:
       - save them somewhere to be accessible by '/list' command
    */
    
  } else {

    /* unknown command */
    nc_log_writef("info", "Incoming discovery (UDP) uknown command: %s", body);
    
  }
}
