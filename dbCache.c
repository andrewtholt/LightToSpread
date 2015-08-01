#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>

#include <sqlite3.h>

#include "connectToSpread.h"
#include "hash.h"
#include "sp.h"
#include "mine.h"

#define MAX_MESSLEN     102400
#define BUFFSIZE 255

typedef enum {UNKNOWN, CONNECTED, JOINED, LEFT,  DISCONNECTED, NETWORK} connectionStatus;

void statusToText( int status, char *data ) {
  switch(status) {
    case UNKNOWN:
      strcpy(data,"UNKNOWN");
      break;
    case CONNECTED:
      strcpy(data,"CONNECTED");
      break;
    case JOINED:
      strcpy(data,"JOINED");
      break;
    case LEFT:
      strcpy(data,"LEFT");
      break;
    case DISCONNECTED:
      strcpy(data,"DISCONNECTED");
      break;
    case NETWORK:
      strcpy(data,"NETWORK");
      break;
    default:
      strcpy(data,"UNKNOWN");
      break;
  }
}
struct globalDefinitions global;

void usage() {
  printf("\nUsage: dbCache -h|-c <file>|-d\n\n");
  printf("\t-h\t\tHelp\n");
  printf("\t-c <file>\tConfig file to use.\n");
  printf("\t-d\t\tDebug messages.\n");
}

int main(int argc, const char *argv[]) {
  char from[MAX_GROUP_NAME];
  char message[MAX_MESSLEN];
  membership_info memb_info;
  int num_groups;
  char target_groups[100][MAX_GROUP_NAME];
  int16 mess_type;
  int endian_mismatch;
  
  int service_type;
  int loop = 1;
  int rc;
  int ret;
  int i;
  
  sqlite3 *mydb;
  char *dbName;
  char *zErrMsg;
  
  connectionStatus status = UNKNOWN;
  
  char buffer[MAX_GROUP_NAME];
  char sqlBuffer[1024];
  char *member;
  char *host;
  char *tok;
  char ch;
  extern char *optarg;

  char scratch[255];
  
  global.connected=0;
  global.Group=(char *)NULL;
  global.configFileName=(char *)NULL;
  global.locked = 0;
  global.rawClient =1;
  global.debug = 0;
  
#ifdef FICL
  global.appDir=(char *)NULL;
#endif

  char *group=(char *)NULL;
  
  setSymbol("BUILD",__DATE__,LOCK,LOCAL);
  setSymbol("CLIENT","raw",UNLOCK,GLOBAL);
  setSymbol("SPREAD_SERVER","4803",UNLOCK,GLOBAL);
  setSymbol("GROUP","global",UNLOCK,GLOBAL);
  setSymbol("USER","dbCache",LOCK,GLOBAL);
  setSymbol("DEBUG","false",UNLOCK,GLOBAL);
  setSymbol("MODE","local",UNLOCK,GLOBAL);
  setSymbol("START_FILE","./dbCache.rc",UNLOCK,GLOBAL);
  
  while((ch = getopt(argc, ( char **)argv,"dhc:")) != -1) {
    switch(ch) {
      case 'c':
	setSymbol("START_FILE", optarg,LOCK,GLOBAL);
	break;
      case 'd':
	global.debug=1;
	break;
      case 'h':
	usage();
	exit(0);
	break;
    }
  }
  
  loadFile(getSymbol("START_FILE"));
  
  if( global.debug ) {
    setBoolean("DEBUG",1);
  }
  
  global.debug = getBoolean("DEBUG");
  
  if(global.debug) {
    dumpGlobals();
    dumpSymbols();
  }
  
  dbName = getSymbol("DATABASE");
  
  connectToSpread();
  
  while(loop) {
    status = UNKNOWN;
    ret = SP_receive(global.Mbox, &service_type, from, 100,
		     &num_groups, target_groups,
		     &mess_type, &endian_mismatch, sizeof(message), message);
    
    if (Is_membership_mess (service_type)) {
      ret = SP_get_memb_info(message, service_type, &memb_info);
      
      if (Is_caused_join_mess(service_type)) {
	status=JOINED;
      } 
      if (Is_caused_leave_mess (service_type)) {
	status = LEFT;
      } 
      if (Is_caused_disconnect_mess (service_type)) {
	status = DISCONNECTED;
      } 
      if (Is_caused_network_mess (service_type)) {
	status = NETWORK;
      }
      
      rc = sqlite3_open(dbName, &mydb);
      for (i = 0; i < num_groups; i++) {
	
	if( global.debug) {
	  printf("\t%s\n",(char *) &target_groups[i][0]);
	}
	
	strcpy(buffer, &target_groups[i][1]);
	
	member=strtok(buffer,"#");
	host=strtok(NULL,"#");
	
	if( global.debug) {
	printf("\t\tMember:%s\n", member);
	printf("\t\tHost  :%s\n", host);
	}
	
	statusToText( status, scratch );
	
	if (JOINED == status ) {
	  sprintf( sqlBuffer,"insert or replace into status  ( member, host, grp, state ) values ( '%s','%s','%s','%s');", member, host,from,scratch);
	}
	if( global.debug) {
	  printf ("%s\n",sqlBuffer);
	}
	rc = sqlite3_exec(mydb, sqlBuffer, 0, 0, &zErrMsg);
      }
      strcpy(buffer, &memb_info.changed_member[1]);
      member=strtok(buffer,"#");
      host=strtok(NULL,"#");
      
      statusToText( status, scratch );
      sprintf( sqlBuffer, "update status set state='%s' where member = '%s' and host ='%s';",scratch, member,host);
      
      if( global.debug) {
	printf("CHANGE: %s %s\n",member,host);
	printf("CHANGE: %s\n", scratch);
	printf ("%s\n",sqlBuffer);
      }
      
      rc = sqlite3_exec(mydb, sqlBuffer, 0, 0, &zErrMsg);
      
      sqlite3_close(mydb);
      
    }
    
  }
  
  
  printf("============ After\n");
  dumpGlobals();
  
  printf("Sender %s\n",from);
  printf("Message >%s<\n",message);
  
  toSpread(from,"Message recieved\n");
  
  spreadJoin("TEST");
  
  sleep(2);
  
  spreadLeave("TEST");
  
  sleep(2);
  
  spreadDisconnect();
}
