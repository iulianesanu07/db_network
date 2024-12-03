#include "sql_queries.h"
#include <arpa/inet.h>
#include <libpq-fe.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

#define PROTOCOL_VERSION 1

// Codes de reponse
#define ACCESS_DENIED 0
#define ACCESS_GRANTED 1

// Types de requetes
#define REQUEST_CHECK_BADGE 1
#define REQUEST_MAINTENANCE_CHECK 2

#define DB_HOST "localhost"
#define DB_PORT "5432"
#define DB_USER "iulianesanu"
// #define DB_PASSWORD "Let's_go"
#define DB_NAME "db_reseau"

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PENDING_CONNECTIONS 5

#define MAX_CLIENTS 10
#define CLIENT_TIMEOUT_SEC 10

// Structure pour le contexte du serveur
typedef struct {
  int server_socket;
} ServerContext;

// Structure pour les arguments du thread client
typedef struct {
  int client_socket;
  struct sockaddr_in client_addr;
} ClientThreadArgs;

// Prototypes des fonctions
int initialize_server(ServerContext *context);
void close_server(ServerContext *context);
void *handle_client_thread(void *args);
int check(int exp, const char *msg);
PGconn *connect_db();
int all_tables_exist(PGconn *db_conn);
void process_client_request(PGconn *db_conn, const uint8_t *request,
                            size_t request_length, uint8_t *response,
                            size_t *response_length);
void safe_exit(ServerContext *context, int exit_code);
void log_error(const char *msg);
int check_badge_exists(PGconn *db_conn, const char *badge_id);
int create_all_tables(PGconn *db_conn);

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
    snprintf(conninfo, sizeof(conninfo), "host=%s port=%s dbname=%s user=%s",
             DB_HOST, DB_PORT, DB_NAME, DB_USER);

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
    // Create server socket
    context->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (check(context->server_socket, "Erreur de création du socket serveur") == -1) {
        return -1;
    }

    // Configure server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind socket to port
    if (check(bind(context->server_socket, (struct sockaddr *)&server_addr,
                   sizeof(server_addr)),
              "Erreur de liaison du socket au port") == -1) {
        close(context->server_socket);
        return -1;
    }

    // Start listening
    if (check(listen(context->server_socket, MAX_PENDING_CONNECTIONS),
              "Erreur de mise en écoute du serveur") == -1) {
        close(context->server_socket);
        return -1;
    }

    printf("Serveur en écoute sur le port %d...\n", SERVER_PORT);
    return 0;
}

void close_server(ServerContext *context) {
    if (context->server_socket != -1) {
        close(context->server_socket);
    }
}

void safe_exit(ServerContext *context, int exit_code) {
  close_server(context);
  exit(exit_code);
}

void insert_maintenance_result(PGconn *db_conn, const char *id_porte, bool resultat) {
    const char *param_values[2];
    param_values[0] = resultat ? "t" : "f";
    param_values[1] = id_porte;

    PGresult *res = PQexecParams(
        db_conn,
        "INSERT INTO Maintenance (resultat, id_porte) VALUES ($1::boolean, $2)",
        2,
        NULL,
        param_values,
        NULL,
        NULL,
        0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Erreur lors de l'insertion dans Maintenance : %s\n", PQerrorMessage(db_conn));
    }

    PQclear(res);
}

void *maintenance_check(void *arg) {
    ServerContext *context = (ServerContext *)arg;

    while (1) {
        // Attendre CLIENT_TIMEOUT_SEC secondes
        sleep(CLIENT_TIMEOUT_SEC);

        // Connexion à la base de données
        PGconn *db_conn = connect_db();
        if (db_conn == NULL) {
            fprintf(stderr, "Impossible de se connecter à la base de données pour la maintenance.\n");
            continue;
        }

        // Récupérer la liste des portes
        PGresult *res = PQexec(db_conn, "SELECT id_porte, ip_porte, port_porte FROM Porte");
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            fprintf(stderr, "Erreur lors de la récupération des portes : %s\n", PQerrorMessage(db_conn));
            PQclear(res);
            PQfinish(db_conn);
            continue;
        }

        int num_ports = PQntuples(res);
        for (int i = 0; i < num_ports; i++) {
            char *id_porte = PQgetvalue(res, i, 0);
            char *ip_porte = PQgetvalue(res, i, 1);
            int port_porte = atoi(PQgetvalue(res, i, 2));

            // Créer un socket pour se connecter à la porte
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                perror("Erreur de création du socket pour la maintenance");
                continue;
            }

            struct sockaddr_in porte_addr;
            memset(&porte_addr, 0, sizeof(porte_addr));
            porte_addr.sin_family = AF_INET;
            porte_addr.sin_port = htons(port_porte);
            if (inet_pton(AF_INET, ip_porte, &porte_addr.sin_addr) <= 0) {
                fprintf(stderr, "Adresse IP invalide pour la porte %s\n", id_porte);
                close(sock);
                continue;
            }

            // Définir un timeout pour la connexion
            struct timeval timeout;
            timeout.tv_sec = CLIENT_TIMEOUT_SEC;
            timeout.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

            // Tentative de connexion à la porte
            if (connect(sock, (struct sockaddr *)&porte_addr, sizeof(porte_addr)) < 0) {
                // La porte ne répond pas, insérer un résultat négatif dans la table Maintenance
                fprintf(stderr, "Impossible de se connecter à la porte %s (%s:%d)\n", id_porte, ip_porte, port_porte);
                insert_maintenance_result(db_conn, id_porte, false);
                close(sock);
                continue;
            }

            // Envoyer une requête de maintenance
            uint8_t request[8];
            request[0] = PROTOCOL_VERSION;
            request[1] = REQUEST_MAINTENANCE_CHECK;
            uint16_t data_length = 0;
            uint16_t data_length_network_order = htons(data_length);
            memcpy(&request[2], &data_length_network_order, sizeof(uint16_t));
            uint32_t client_id_network_order = htonl(0);
            memcpy(&request[4], &client_id_network_order, sizeof(uint32_t));

            if (send(sock, request, 8, 0) != 8) {
                perror("Erreur lors de l'envoi de la requête de maintenance");
                close(sock);
                continue;
            }

            // Recevoir la réponse
            uint8_t response[BUFFER_SIZE];
            ssize_t bytes_received = recv(sock, response, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                fprintf(stderr, "Pas de réponse de la porte %s\n", id_porte);
                insert_maintenance_result(db_conn, id_porte, false);
                close(sock);
                continue;
            }

            // Traiter la réponse
            uint8_t code = response[0];
            if (code == ACCESS_GRANTED) {
                // La porte est en ligne
                insert_maintenance_result(db_conn, id_porte, true);
            } else {
                // La porte a répondu mais avec une erreur
                insert_maintenance_result(db_conn, id_porte, false);
            }

            close(sock);
        }

        PQclear(res);
        PQfinish(db_conn);
    }

    return NULL;
}

void process_client_request(PGconn *db_conn, const uint8_t *request,
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
    int exists = check_badge_exists(db_conn, badge_id); // Modification ici
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
void *handle_client_thread(void *args) {
    ClientThreadArgs *client_args = (ClientThreadArgs *)args;
    int client_socket = client_args->client_socket;
    struct sockaddr_in client_addr = client_args->client_addr;
    free(client_args);  // Free the allocated memory for arguments

    uint8_t buffer[BUFFER_SIZE];
    uint8_t response[BUFFER_SIZE];
    size_t response_length = 0;

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = CLIENT_TIMEOUT_SEC;
    timeout.tv_usec = 0;

    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                   sizeof(timeout)) < 0) {
        perror("Erreur lors du réglage du timeout du socket");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Each thread needs its own database connection
    PGconn *db_conn = connect_db();
    if (db_conn == NULL) {
        fprintf(stderr, "Impossible de se connecter à la base de données.\n");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Receive data from the client
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            printf("Le client a fermé la connexion.\n");
        } else {
            perror("Erreur de réception des données du client ou timeout atteint");
        }
        PQfinish(db_conn);
        close(client_socket);
        pthread_exit(NULL);
    }

    // Process the client's request
    process_client_request(db_conn, buffer, (size_t)bytes_received, response,
                           &response_length);

    // Send the response to the client
    ssize_t bytes_sent = send(client_socket, response, response_length, 0);
    if (bytes_sent < 0) {
        perror("Erreur d'envoi de la réponse au client");
    }

    PQfinish(db_conn);
    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    ServerContext context;
    context.server_socket = -1;

    // Initialisation du serveur
    if (initialize_server(&context) == -1) {
        safe_exit(&context, EXIT_FAILURE);
    }

    // Lancer le thread de maintenance
    pthread_t maintenance_thread;
    if (pthread_create(&maintenance_thread, NULL, maintenance_check, &context) != 0) {
        perror("Erreur lors de la création du thread de maintenance");
        safe_exit(&context, EXIT_FAILURE);
    }

    // Détacher le thread de maintenance pour qu'il libère ses ressources à la fin
    pthread_detach(maintenance_thread);

    // Boucle principale du serveur pour accepter les connexions des clients
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        // Attendre une connexion entrante
        int client_socket = accept(context.server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Erreur lors de l'acceptation d'une connexion client");
            continue;
        }

        // Allouer de la mémoire pour les arguments du thread client
        ClientThreadArgs *client_args = malloc(sizeof(ClientThreadArgs));
        if (client_args == NULL) {
            perror("Erreur d'allocation de mémoire pour les arguments du thread");
            close(client_socket);
            continue;
        }

        client_args->client_socket = client_socket;
        client_args->client_addr = client_addr;

        // Créer un nouveau thread pour gérer le client
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client_thread, (void *)client_args) != 0) {
            perror("Erreur lors de la création du thread client");
            free(client_args);
            close(client_socket);
            continue;
        }

        // Détacher le thread client pour qu'il libère ses ressources à la fin
        pthread_detach(client_thread);
    }

    // Fermeture du serveur (normalement, on n'arrivera jamais ici)
    safe_exit(&context, EXIT_SUCCESS);
    return 0;
}
