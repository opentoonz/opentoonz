

#include <errno.h> /* obligatory includes */
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include "tcommon.h"
#define MAXHOSTNAME 1024

//----------------------------------------------------------------------------

int call_socket(char *hostname, unsigned short portnum) {
  struct sockaddr_in sa;
  struct hostent *hp;
  int s;

  if ((hp = gethostbyname(hostname)) == NULL) /* do we know the host's */
  {
    errno = ECONNREFUSED; /* address? */
    return (-1);          /* no */
  }

  memset(&sa, 0, sizeof(sa));
  memcpy((char *)&sa.sin_addr, hp->h_addr, hp->h_length); /* set address */
  sa.sin_family = hp->h_addrtype;
  sa.sin_port   = htons((u_short)portnum);

  if ((s = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0) /* get socket */
    return (-1);
  if (connect(s, (struct sockaddr *)&sa, sizeof sa) < 0) /* connect */
  {
    close(s);
    return (-1);
  }

  return (s);
}

//----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  char myname[MAXHOSTNAME + 1];
  gethostname(myname, MAXHOSTNAME); /* who are we? */

  int s;
  int portNumber = 8002;
  {
    std::ifstream is("/tmp/.tfarmserverd.dat");
    is >> portNumber;
  }
  //  std::cout << "shutting down " << portNumber << std::endl;
  if ((s = call_socket(myname, portNumber)) < 0) {
    fprintf(stderr, "Unable to stop the tfarmserved daemon\n");
    exit(1);
  }

  write(s, "shutdown", strlen("shutdown") + 1);

  close(s);
  exit(0);
}
