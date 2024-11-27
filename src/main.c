#include <arpa/inet.h>
#include <libpq-fe.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define HOST "localhost"
#define PORT_DB "5432"
#define USER "iulianesanu"

#define PORT_SRV 8080
#define BUFFER_SIZE 1024

// Acces DataBase 
static PGconn *db;
// Reponse client
char response[BUFFER_SIZE];

// Declaration constantes pour query SQL
#define CREA_TABLE_TEST "CREATE TABLE test_insert_id ( id VARCHAR(100));"
#define INSERT_ID_TABLE_TEST_1 "INSERT INTO test_insert_id (id) Values ("
#define INSERT_ID_TABLE_TEST_2 ");"
#define SELECT_DATA_TABLE_TEST "SELECT * FROM public.test_insert_id;"

// Declaration prototype des fontions
int check(int exp, const char *msg);
void packet_solver(char *buffer);
void test_add_id_db(char *buffer);

// Fonction de verification, permet une meilleur visibilite du code
#define SOCKETERROR (-1)
int check(int exp, const char *msg) {
  if (exp == SOCKETERROR) {
    perror(msg);
    exit(EXIT_FAILURE);
  }

  return exp;
}

// Fonction de connexion a la db
void connectDB() {
  // Paramètres de connexion à adapter
  char conninfo[100];
  sprintf(conninfo, "host=%s port=%s user=%s", HOST, PORT_DB, USER);

  // Create a connection
  db = PQconnectdb(conninfo);

  if (PQstatus(db) != CONNECTION_OK) {
    printf("Error while dbecting to the database : %s\n",
           PQerrorMessage(db));
    PQfinish(db);
    exit(EXIT_FAILURE);
  }

  // Nice t'es connecte
  printf("Connection Established : \n");
  printf("\tPort: %s\n", PQport(db));
  printf("\tHost: %s\n", PQhost(db));
  printf("\tDBName: %s\n\n", PQdb(db));

  return;
}

int main() {
  connectDB();

  // Variables client
  int client_socket;
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_len);

  char buffer[BUFFER_SIZE];

  // Creer un socket TCP
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  check(server_socket, "Erreur de creation de socket TCP");

  // Configuration de l'adresse du serveur
  struct sockaddr_in server_addr = {.sin_family = AF_INET,
                                    .sin_addr.s_addr = INADDR_ANY,
                                    .sin_port = htons(PORT_SRV)};

  // Attachement du socket au port
  check(
      bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)),
      "Erreur de bind");

  // Mise en mode ecoute du serveur
  check(listen(server_socket, 5), "Erreur de mise en ecoute");

  printf("Serveur TCP en ecoute sur le port %d ... \n", PORT_SRV);


  
  // Boucle principale
  while (1) {
    // Accepter une connexion entrante
    client_socket = check(
        accept(server_socket, (struct sockaddr *)&client_addr, &client_len),
        "Erreur d'acceptation");

    // Recevoir un message du client
    int len = read(client_socket, buffer, BUFFER_SIZE);
    if (len < 0) {
      perror("Erreur de lecture");
      close(client_socket);
      continue;
    }

    buffer[len] = '\0'; // Ajouter un terminateur a la fin du message transmis
                        // par le client
    printf("Message recu : %s\n", buffer);

    // Resolution de message
    packet_solver(buffer);

    // Envoyer une reponse au client
    write(client_socket, response, strlen(response));

    close(client_socket); // Fermer la connexion avec ce client
  }

  close(server_socket);
  PQfinish(db);

  return EXIT_SUCCESS;
}

// Determine quoi faire avec chaque packet recu de la part d'un client
// en fonction de son type.
// A completer au fur et a mesure
void packet_solver(char *buffer) {

  test_add_id_db(buffer);

  return;
}

// La c'est explicite quand meme x)
void print_table(PGresult *res) {
  int rows = PQntuples(res);
  int cols = PQnfields(res);
  printf("Number of rows : %d\n", rows);
  printf("Number of columns : %d\n", cols);

  // Print the column names
  for (int i = 0; i < cols; i++) {
    printf("%s\t", PQfname(res, i));
  }
  printf("\n");

  // Print all the rows and columns
  for (int i = 0; i < rows; i++) {


    for (int j = 0; j < cols; j++) {
      // Print the column value
      printf("%s\t", PQgetvalue(res, i, j));
    }
    printf("\n");
  }

  return;
}

// Decris l'erreur si il y en a eu une, pour eviter les repetitions de code
void check_query(PGresult *res) {
  ExecStatusType resStatus = PQresultStatus(res);
  printf("\n%s\n", PQresStatus(resStatus));
  if (resStatus != PGRES_COMMAND_OK) {
    printf("%s\n", PQerrorMessage(db));
    return;
  }
  return;
}

void set_response(char * r) {
  printf("len de r : %d", (int)strlen(r));
  for (int i = 0; i < (int)strlen(r); i++) {
    response[i] = r[i];
  }

  response[strlen(r)] = '\0';

  return;
}

void test_add_id_db(char *buffer) {

  // Submit the query and retreive the result
  PGresult *res = PQexec(db, CREA_TABLE_TEST);
  check_query(res);

  char query[254];
  snprintf(query, sizeof(query), "%s'%s'%s", INSERT_ID_TABLE_TEST_1, buffer, INSERT_ID_TABLE_TEST_2);
//  printf("%s\n",query);
  res = PQexec(db, query);
  check_query(res);

  // Test fetch from db
  char *query2 = SELECT_DATA_TABLE_TEST;
  res = PQexec(db, query2);
  check_query(res);
  print_table(res);
  set_response((char *)PQresultStatus(res)); // segmentation fault

  return;
}
