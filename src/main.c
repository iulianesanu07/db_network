#include "sql_queries.h"
#include <arpa/inet.h>
#include <libpq-fe.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PROTOCOL_VERSION 1

// Codes de reponse
#define ACCESS_DENIED 0
#define ACCESS_GRANTED 1

// Types de requetes
#define REQUEST_CHECK_BADGE 1

#define DB_HOST "localhost"
#define DB_PORT "5432"
#define DB_USER "iulianesanu"
// #define DB_PASSWORD "Let's_go"
#define DB_NAME "db_reseau"

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PENDING_CONNECTIONS 5

// Structure pour le contexte du serveur
typedef struct {
  PGconn *db_conn;
  int server_socket;
} ServerContext;

// Prototypes des fonctions
int initialize_server(ServerContext *context);
void close_server(ServerContext *context);
int handle_client_connection(ServerContext *context, int client_socket);
int check(int exp, const char *msg);
PGconn *connect_db();
int all_tables_exist(PGconn *db_conn);
void get_all_table_names(PGconn *db_conn);
void process_client_request(ServerContext *context, const uint8_t *request,
                            size_t request_length, uint8_t *response,
                            size_t *response_length);
void safe_exit(ServerContext *context, int exit_code);
void log_error(const char *msg);
int check_badge_exists(PGconn *db_conn, const char *badge_id);

// Fonctions utilitaires
int check(int exp, const char *msg) {
  if (exp == -1) {
    perror(msg);
    return -1;
  }
  return exp;
}

void log_error(const char *msg) { fprintf(stderr, "%s\n", msg); }

PGconn *connect_db() {
  char conninfo[256];
  snprintf(conninfo, sizeof(conninfo), "host=%s port=%s dbname=%s", DB_HOST,
           DB_PORT, DB_NAME);
  //    snprintf(conninfo, sizeof(conninfo),
  //            "host=%s port=%s dbname=%s user=%s password=%s",
  //             DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD);

  PGconn *db_conn = PQconnectdb(conninfo);

  if (PQstatus(db_conn) != CONNECTION_OK) {
    log_error("Erreur de connexion à la base de données :");
    log_error(PQerrorMessage(db_conn));
    PQfinish(db_conn);
    return NULL;
  }

  printf("Connexion à la base de données établie avec succès.\n");
  return db_conn;
}

int create_all_tables(PGconn *db_conn) {
  const char *create_table_queries[] = {
      CREATE_TABLE_BADGE,          CREATE_TABLE_ROLE,
      CREATE_TABLE_INDIVIDU,       CREATE_TABLE_PORTE,
      CREATE_TABLE_HISTORIQUE,     CREATE_TABLE_EMPLOYE,
      CREATE_TABLE_VISITEUR,       CREATE_TABLE_MAINTENANCE,
      CREATE_TABLE_ESPACE,         CREATE_TABLE_ZONE_SECURISE,
      CREATE_TABLE_SE_TROUVER_DANS};
  const char *table_names[] = {"badge",         "role",           "individu",
                               "porte",         "historique",     "employe",
                               "visiteur",      "maintenance",    "espace",
                               "zone_securise", "se_trouver_dans"};
  int num_tables =
      sizeof(create_table_queries) / sizeof(create_table_queries[0]);

  for (int i = 0; i < num_tables; i++) {
    PGresult *res = PQexec(db_conn, create_table_queries[i]);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      fprintf(stderr, "Erreur lors de la création de la table '%s' : %s\n",
              table_names[i], PQerrorMessage(db_conn));
      PQclear(res);
      return -1; // Indique une erreur
    } else {
      printf("Table '%s' créée ou déjà existante.\n", table_names[i]);
    }

    PQclear(res);
  }

  return 0; // Succès
}

int all_tables_exist(PGconn *db_conn) {
  const char *required_tables[] = {
      "badge",      "role",          "individu",       "porte",
      "historique", "employe",       "visiteur",       "maintenance",
      "espace",     "zone_securise", "se_trouver_dans"};
  int num_required_tables =
      sizeof(required_tables) / sizeof(required_tables[0]);

  const char *query =
      "SELECT table_name FROM information_schema.tables "
      "WHERE table_schema = 'public' AND table_type = 'BASE TABLE';";

  PGresult *res = PQexec(db_conn, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    log_error("Erreur lors de la récupération des noms des tables :");
    log_error(PQerrorMessage(db_conn));
    PQclear(res);
    return -1;
  }

  int num_existing_tables = PQntuples(res);
  char **existing_tables = malloc(num_existing_tables * sizeof(char *));
  if (existing_tables == NULL) {
    log_error("Erreur d'allocation de mémoire.");
    PQclear(res);
    return -1;
  }

  // Stockage des noms de tables existantes
  for (int i = 0; i < num_existing_tables; i++) {
    char *table_name = PQgetvalue(res, i, 0);
    existing_tables[i] = strdup(table_name);
    if (existing_tables[i] == NULL) {
      log_error("Erreur d'allocation de mémoire.");
      // Libération de la mémoire allouée précédemment
      for (int j = 0; j < i; j++) {
        free(existing_tables[j]);
      }
      free(existing_tables);
      PQclear(res);
      return -1;
    }
  }

  PQclear(res);

  // Vérification de l'existence de chaque table requise
  for (int i = 0; i < num_required_tables; i++) {
    int found = 0;
    for (int j = 0; j < num_existing_tables; j++) {
      if (strcasecmp(required_tables[i], existing_tables[j]) == 0) {
        found = 1;
        break;
      }
    }
    if (!found) {
      fprintf(stderr,
              "La table requise '%s' n'existe pas dans la base de données.\n",
              required_tables[i]);
      // Libération de la mémoire
      for (int j = 0; j < num_existing_tables; j++) {
        free(existing_tables[j]);
      }
      free(existing_tables);
      return 0; // Une table est manquante
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

// Fonction pour verifier si un badge existe
int check_badge_exists(PGconn *db_conn, const char *badge_id) {
  const char *param_values[1] = {badge_id};
  PGresult *res = PQexecParams(
      db_conn, "SELECT 1 FROM Badge WHERE id_badge = $1",
      1,    // Nombre de paramètres
      NULL, // Types des paramètres (NULL pour laisser PostgreSQL déduire)
      param_values,
      NULL, // Tailles des paramètres (NULL pour chaînes de caractères)
      NULL, // Formats des paramètres (NULL pour texte)
      0);   // Format du résultat (0 pour texte)

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "Erreur lors de la vérification du badge : %s\n",
            PQerrorMessage(db_conn));
    PQclear(res);
    return -1; // Indique une erreur
  }

  int rows = PQntuples(res);
  PQclear(res);

  return rows > 0 ? 1 : 0; // 1 si le badge existe, 0 sinon
}

int initialize_server(ServerContext *context) {
  // Initialisation de la connexion à la base de données
  context->db_conn = connect_db();
  if (context->db_conn == NULL) {
    return -1;
  }

  // Vérification de l'existence des tables requises
  int tables_exist = all_tables_exist(context->db_conn);
  if (tables_exist != 1) {
    PQfinish(context->db_conn);
    return -1;
  }

  // Création du socket serveur
  context->server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (check(context->server_socket, "Erreur de création du socket serveur") ==
      -1) {
    PQfinish(context->db_conn);
    return -1;
  }

  // Configuration de l'adresse du serveur
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(SERVER_PORT);

  // Liaison du socket au port
  if (check(bind(context->server_socket, (struct sockaddr *)&server_addr,
                 sizeof(server_addr)),
            "Erreur de liaison du socket au port") == -1) {
    close(context->server_socket);
    PQfinish(context->db_conn);
    return -1;
  }

  // Mise en écoute du serveur
  if (check(listen(context->server_socket, MAX_PENDING_CONNECTIONS),
            "Erreur de mise en écoute du serveur") == -1) {
    close(context->server_socket);
    PQfinish(context->db_conn);
    return -1;
  }

  printf("Serveur en écoute sur le port %d...\n", SERVER_PORT);
  return 0;
}

void close_server(ServerContext *context) {
  if (context->server_socket != -1) {
    close(context->server_socket);
  }
  if (context->db_conn != NULL) {
    PQfinish(context->db_conn);
  }
}

void safe_exit(ServerContext *context, int exit_code) {
  close_server(context);
  exit(exit_code);
}
void process_client_request(ServerContext *context, const uint8_t *request,
                            size_t request_length, uint8_t *response,
                            size_t *response_length) {
  if (request_length < 8) {
    fprintf(stderr, "Requête invalide : trop courte.\n");
    response[0] = ACCESS_DENIED;
    const char *message = "Requête invalide.";
    strcpy((char *)&response[1], message);
    *response_length = 1 + strlen(message);
    return;
  }

  // Extraction de l'en-tête
  uint8_t version = request[0];
  uint8_t request_type = request[1];

  uint16_t data_length_network_order;
  memcpy(&data_length_network_order, &request[2], sizeof(uint16_t));
  uint16_t data_length = ntohs(data_length_network_order);

  uint32_t client_id_network_order;
  memcpy(&client_id_network_order, &request[4], sizeof(uint32_t));
  uint32_t client_id = ntohl(client_id_network_order);
  // Vérification de la version du protocole
  if (version != PROTOCOL_VERSION) {
    fprintf(stderr, "Version de protocole incompatible.\n");
    response[0] = ACCESS_DENIED;
    const char *message = "Version de protocole incompatible.";
    strcpy((char *)&response[1], message);
    *response_length = 1 + strlen(message);
    return;
  }

  // Vérification de la longueur des données
  if (request_length - 8 != data_length) {
    fprintf(stderr, "Longueur des données incorrecte.\n");
    response[0] = ACCESS_DENIED;
    const char *message = "Longueur des données incorrecte.";
    strcpy((char *)&response[1], message);
    *response_length = 1 + strlen(message);
    return;
  }

  // Traitement en fonction du type de requête
  switch (request_type) {
  case REQUEST_CHECK_BADGE: {
    // Extraction des données (ID du badge)
    size_t badge_id_length =
        data_length; // La longueur des données est celle de l'ID du badge
    if (badge_id_length >= 256) {
      fprintf(stderr, "ID de carte trop long.\n");
      response[0] = ACCESS_DENIED;
      const char *message = "ID de carte trop long.";
      strcpy((char *)&response[1], message);
      *response_length = 1 + strlen(message);
      return;
    }

    char badge_id[256];
    memcpy(badge_id, &request[8], badge_id_length); // Copier depuis request[8]
    badge_id[badge_id_length] = '\0';

    // Pour information, on peut utiliser l'identifiant client (client_id) si
    // nécessaire
    printf("Requête de vérification du badge '%s' pour le client ID %u\n",
           badge_id, client_id);

    // Vérifier si le badge existe
    int exists = check_badge_exists(context->db_conn, badge_id);
    if (exists == -1) {
      // Erreur lors de la vérification
      response[0] = ACCESS_DENIED;
      const char *message = "Erreur du serveur.";
      strcpy((char *)&response[1], message);
      *response_length = 1 + strlen(message);
      return;
    }

    if (exists == 1) {
      // Badge existe, accès autorisé
      response[0] = ACCESS_GRANTED;
      const char *message = "Accès autorisé.";
      strcpy((char *)&response[1], message);
      *response_length = 1 + strlen(message);
    } else {
      // Badge n'existe pas, accès refusé
      response[0] = ACCESS_DENIED;
      const char *message = "Accès refusé.";
      strcpy((char *)&response[1], message);
      *response_length = 1 + strlen(message);
    }
    break;
  }
  default:
    fprintf(stderr, "Type de requête inconnu : %u\n", request_type);
    response[0] = ACCESS_DENIED;
    const char *message = "Type de requête inconnu.";
    strcpy((char *)&response[1], message);
    *response_length = 1 + strlen(message);
    break;
  }
}
int handle_client_connection(ServerContext *context, int client_socket) {
  uint8_t buffer[BUFFER_SIZE];
  uint8_t response[BUFFER_SIZE];
  size_t response_length = 0;

  // Réception de la requête du client
  ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
  if (bytes_received <= 0) {
    perror("Erreur de réception des données du client");
    close(client_socket);
    return -1;
  }

  // Traitement de la requête
  process_client_request(context, buffer, (size_t)bytes_received, response,
                         &response_length);

  // Envoi de la réponse au client
  ssize_t bytes_sent = send(client_socket, response, response_length, 0);
  if (bytes_sent < 0) {
    perror("Erreur d'envoi de la réponse au client");
    close(client_socket);
    return -1;
  }

  close(client_socket);
  return 0;
}

int main() {
  ServerContext context;
  context.db_conn = NULL;
  context.server_socket = -1;

  // Initialisation de la connexion à la base de données
  context.db_conn = connect_db();
  if (context.db_conn == NULL) {
    fprintf(stderr, "Impossible de se connecter à la base de données.\n");
    return EXIT_FAILURE;
  }

  // Vérification de l'existence des tables
  int tables_exist = all_tables_exist(context.db_conn);
  if (tables_exist == -1) {
    // Erreur lors de la vérification
    PQfinish(context.db_conn);
    return EXIT_FAILURE;
  } else if (tables_exist == 0) {
    // Les tables n'existent pas, on les crée
    printf("Les tables n'existent pas, création des tables...\n");
    if (create_all_tables(context.db_conn) != 0) {
      fprintf(stderr, "Erreur lors de la création des tables.\n");
      PQfinish(context.db_conn);
      return EXIT_FAILURE;
    } else {
      printf("Toutes les tables ont été créées avec succès.\n");
    }
  }

  // Initialisation du serveur
  if (initialize_server(&context) == -1) {
    safe_exit(&context, EXIT_FAILURE);
  }

  // Boucle principale du serveur
  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int client_socket =
        accept(context.server_socket, (struct sockaddr *)&client_addr,
               &client_addr_len);
    if (client_socket == -1) {
      perror("Erreur lors de l'acceptation d'une connexion client");
      continue;
    }

    // Gestion de la connexion client
    if (handle_client_connection(&context, client_socket) == -1) {
      // En cas d'erreur, on continue avec la prochaine connexion
      continue;
    }
  }

  // Fermeture du serveur (normalement, on n'arrivera jamais ici)
  safe_exit(&context, EXIT_SUCCESS);
  return 0;
}
