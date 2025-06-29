#ifndef DATA_H
#define DATA_H

#define SHUTDOWN_EV _T("Global\\ShutdownEvent") 
#define SHARED_MEM _T("Local\\playersTPSO2")
#define PIPE_NAME _T("\\\\.\\pipe\\TPSO2Pipe")
#define GAMESTART_EV _T("Global\\GameStartEvent")
#define SHARED_MEM_SIZE 4024

#define MAX_PLAYERS 20
#define BUFFER 1024
#define MAX_USERNAME 20
#define MAX_WORD_LENGTH 50
#define MAX_LETRAS 6

//	Manager response/messages
#define ERROR_MSG 6 //	Error (sent by manager)
#define START_MSG 7 //	Game starts
#define KICK_MSG 8 //	User Kick
#define LIST_RESP 9 // List response message
#define REFRESH_LETTERS 10 // Order to get the new letters fom shared memory
#define DICTIONARY 99

//	Player requests/messages
#define JOIN_MSG 0 //	Player join
#define JOIN_MSG_B 1
#define EXIT_MSG 2 //	Player disconnect
#define LIST_REQ 3 //	Player requests list of players
#define	WORD_MSG 4 //	Player word guess
#define SCORE_REQ 5 //	Player requests his score (:pont)

//	Standard messages
#define NORMAL_MSG 11 //	Any message, eg. "a player joined" message
#define MANAGER_SHUTDOWN -1 // Manager deu shutdown

// Registry keys
#define REGISTRY_KEY _T("Software\\TrabSO2")
#define REGISTRY_MAX_LETTERS _T("MAXLETRAS")
#define REGISTRY_RHYTHM _T("RITMO")
#define REGISTRY_NO_MAX -10
#define REGISTRY_FASTER _T('f')
#define REGISTRY_SLOWER _T('s')

// Player structure
typedef struct {
	TCHAR userName[MAX_USERNAME];
	float score;
	BOOL isBot;
	HANDLE hPipe;
	HANDLE hThread;
	HANDLE hExitEvent;
} Player;

typedef struct {
	int type;
	TCHAR userName[MAX_USERNAME];
	TCHAR content[BUFFER];
} Message;

typedef struct {
	TCHAR letters[MAX_LETRAS];
	int letterCount;
	int orderLetters[MAX_LETRAS];
	char lastWord[MAX_WORD_LENGTH];
	Player players[MAX_PLAYERS];
	int playerCount;
	BOOL gameRunning;
} SharedData;

#endif // DATA_H