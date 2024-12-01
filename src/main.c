#include <arpa/inet.h>
#include <libpq-fe.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "sql_queries.h"

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
void get_all_table_names(PGconn *db);
int all_tables_exist(PGconn *db);



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

  // Appel de la fonction all_tables_exist()
    int result = all_tables_exist(db);
    if (result == -1) {
        // Erreur lors de la vérification
        PQfinish(db);
        return EXIT_FAILURE;
    } else if (result == 0) {
        // Une ou plusieurs tables sont manquantes
        PQfinish(db);
        return EXIT_FAILURE;
    }

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

void set_response(const char * r) {
  strncpy(response, r, BUFFER_SIZE -1);
  response[BUFFER_SIZE -1] = '\0'; 
}

void test_add_id_db(char *buffer) {
    PGresult *res;

    char query[254];
    snprintf(query, sizeof(query), "%s'%s'%s", INSERT_ID_TABLE_TEST_1, buffer, INSERT_ID_TABLE_TEST_2);
    res = PQexec(db, query);
    check_query(res);
    PQclear(res);

    // Test fetch from db
    res = PQexec(db, SELECT_DATA_TABLE_TEST);
    check_query(res);
    print_table(res);

    // Get the execution status as a string
    ExecStatusType resStatus = PQresultStatus(res);
    const char *statusMessage = PQresStatus(resStatus);
    set_response(statusMessage);
    PQclear(res); // Clear the result

    return;
}

void get_all_table_names(PGconn *db) {
    const char *query =
        "SELECT table_name FROM information_schema.tables "
        "WHERE table_schema = 'public' AND table_type = 'BASE TABLE';";

    PGresult *res = PQexec(db, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Erreur lors de la récupération des noms des tables : %s\n", PQerrorMessage(db));
        PQclear(res);
        return;
    }

    int rows = PQntuples(res);

    printf("Liste des tables existantes dans le schéma 'public' :\n");

    for (int i = 0; i < rows; i++) {
        char *table_name = PQgetvalue(res, i, 0);
        printf("%s\n", table_name);
    }

    PQclear(res);
}

int all_tables_exist(PGconn *db) {
    // Liste des tables requises
    const char *required_tables[] = {
        "badge",
        "role",
        "individu",
        "porte",
        "historique",
        "employe",
        "visiteur",
        "maintenance",
        "espace",
        "zone_securise",
        "se_trouver_dans"
    };
    int num_required_tables = sizeof(required_tables) / sizeof(required_tables[0]);

    // Requête pour récupérer tous les noms de tables existantes dans le schéma 'public'
    const char *query =
        "SELECT table_name FROM information_schema.tables "
        "WHERE table_schema = 'public' AND table_type = 'BASE TABLE';";

    PGresult *res = PQexec(db, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Erreur lors de la récupération des noms des tables : %s\n", PQerrorMessage(db));
        PQclear(res);
        return -1; // Indique une erreur
    }

    int num_existing_tables = PQntuples(res);

    // Tableau pour stocker les noms des tables existantes
    char **existing_tables = malloc(num_existing_tables * sizeof(char *));
    if (existing_tables == NULL) {
        fprintf(stderr, "Erreur d'allocation de mémoire.\n");
        PQclear(res);
        return -1;
    }

    // Récupération des noms de tables existantes et conversion en minuscules
    for (int i = 0; i < num_existing_tables; i++) {
        char *table_name = PQgetvalue(res, i, 0);
        // Conversion en minuscules
        int len = strlen(table_name);
        existing_tables[i] = malloc((len + 1) * sizeof(char));
        if (existing_tables[i] == NULL) {
            fprintf(stderr, "Erreur d'allocation de mémoire.\n");
            // Libération de la mémoire allouée précédemment
            for (int j = 0; j < i; j++) {
                free(existing_tables[j]);
            }
            free(existing_tables);
            PQclear(res);
            return -1;
        }
        for (int j = 0; j < len; j++) {
            existing_tables[i][j] = table_name[j];
        }
        existing_tables[i][len] = '\0';
    }

    PQclear(res);

    // Vérification de l'existence de chaque table requise
    for (int i = 0; i < num_required_tables; i++) {
        // Conversion du nom de la table requise en minuscules
        char required_table_lower[256];
        int len = strlen(required_tables[i]);
        if (len >= (int)sizeof(required_table_lower)) {
            fprintf(stderr, "Nom de table trop long : %s\n", required_tables[i]);
            // Libération de la mémoire
            for (int j = 0; j < num_existing_tables; j++) {
                free(existing_tables[j]);
            }
            free(existing_tables);
            return -1;
        }
        for (int j = 0; j < len; j++) {
            required_table_lower[j] = required_tables[i][j];
        }
        required_table_lower[len] = '\0';

        int found = 0;
        for (int j = 0; j < num_existing_tables; j++) {
            if (strcmp(required_table_lower, existing_tables[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "La table requise '%s' n'existe pas dans la base de données.\n", required_tables[i]);
            // Libération de la mémoire
            for (int j = 0; j < num_existing_tables; j++) {
                free(existing_tables[j]);
            }
            free(existing_tables);
            return 0; // Une table manquante
        }
    }

    // Toutes les tables requises existent
    printf("Toutes les tables requises existent dans la base de données.\n");

    // Libération de la mémoire
    for (int i = 0; i < num_existing_tables; i++) {
        free(existing_tables[i]);
    }
    free(existing_tables);

    return 1; // Succès
}
