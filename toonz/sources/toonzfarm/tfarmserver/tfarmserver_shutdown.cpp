

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
  struct addrinfo *ai = NULL;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  int s;

  int suc = getaddrinfo(hostname, NULL, &hints, &ai);
  if (suc != 0) /* do we know the host's */
  {
    errno = ECONNREFUSED; /* address? */
    return (-1);          /* no */
  }

  struct sockaddr_in *ai_addr;
  ai_addr = (struct sockaddr_in *)ai->ai_addr;

  memset(&sa, 0, sizeof(sa));
  sa.sin_addr = ai_addr->sin_addr;
  sa.sin_family = ai_addr->sin_family;
  sa.sin_port   = htons((u_short)portnum);

  if ((s = socket(ai_addr->sin_family, SOCK_STREAM, 0)) < 0) /* get socket */
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
