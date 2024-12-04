#include <arpa/inet.h>
#include <libpq-fe.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// Définition des constantes
#define PROTOCOL_VERSION 1

// Codes de réponse
#define ACCESS_DENIED 0
#define ACCESS_GRANTED 1
#define PASSWORD_REQUIRED 2

// Types de requêtes
#define REQUEST_CHECK_BADGE 1
#define REQUEST_MAINTENANCE_SIGNAL 2
#define REQUEST_SEND_PASSWORD 3

// Informations de connexion à la base de données
#define DB_HOST "localhost"
#define DB_PORT "5432"
#define DB_USER "iulianesanu"
#define DB_PASSWORD "Let's_go"
#define DB_NAME "db_reseau"

// Configuration du serveur
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PENDING_CONNECTIONS 5

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
void process_client_request(PGconn *db_conn, const uint8_t *request, size_t request_length, uint8_t *response, size_t *response_length, const char *client_ip);
void safe_exit(ServerContext *context, int exit_code);
void log_error(const char *msg);
void insert_maintenance_result(PGconn *db_conn, const char *id_porte_str, bool resultat);
void insert_historique(PGconn *db_conn, const char *badge_id_str, uint32_t id_porte, int resultat);

// Fonctions utilitaires
int check(int exp, const char *msg) {
    if (exp == -1) {
        perror(msg);
        return -1;
    }
    return exp;
}

void log_error(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

PGconn *connect_db() {
    char conninfo[256];
    snprintf(conninfo, sizeof(conninfo), "host=%s port=%s dbname=%s user=%s password=%s",
             DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD);

    PGconn *db_conn = PQconnectdb(conninfo);

    if (PQstatus(db_conn) != CONNECTION_OK) {
        log_error("Erreur de connexion à la base de données :");
        log_error(PQerrorMessage(db_conn));
        PQfinish(db_conn);
        return NULL;
    }

    return db_conn;
}

void insert_historique(PGconn *db_conn, const char *badge_id_str, uint32_t id_porte, int resultat) {
    const char *param_values[4];
    char id_porte_str[16];
    snprintf(id_porte_str, sizeof(id_porte_str), "%u", id_porte);

    // Obtenir la date et l'heure actuelles
    char dateheure_str[64];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(dateheure_str, sizeof(dateheure_str), "%Y-%m-%d %H:%M:%S", t);

    param_values[0] = resultat ? "1" : "0";
    param_values[1] = dateheure_str;
    param_values[2] = badge_id_str;
    param_values[3] = id_porte_str;

    PGresult *res = PQexecParams(
        db_conn,
        "INSERT INTO Historique (resultat, dateheure, id_badge, id_porte) VALUES ($1::integer, $2::timestamp, $3::integer, $4::integer)",
        4,
        NULL,
        param_values,
        NULL,
        NULL,
        0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Erreur lors de l'insertion dans Historique : %s\n", PQerrorMessage(db_conn));
    }

    PQclear(res);
}

void insert_maintenance_result(PGconn *db_conn, const char *id_porte_str, bool resultat) {
    const char *param_values[2];
    param_values[0] = resultat ? "t" : "f";
    param_values[1] = id_porte_str;

    PGresult *res = PQexecParams(
        db_conn,
        "INSERT INTO Maintenance (resultat, id_porte) VALUES ($1::boolean, $2::integer)",
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

void process_client_request(PGconn *db_conn, const uint8_t *request,
                            size_t request_length, uint8_t *response,
                            size_t *response_length, const char *client_ip) {
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
        size_t badge_id_length = data_length; // La longueur des données est celle de l'ID du badge
        if (badge_id_length >= 256) {
            fprintf(stderr, "ID de badge trop long.\n");
            response[0] = ACCESS_DENIED;
            const char *message = "ID de badge trop long.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);
            return;
        }

        char badge_id_str[256];
        memcpy(badge_id_str, &request[8], badge_id_length); // Copier depuis request[8]
        badge_id_str[badge_id_length] = '\0';

        int badge_id = atoi(badge_id_str);

        // Affichage pour information
        printf("Requête de vérification du badge '%d' pour le client ID %u\n",
               badge_id, client_id);

        // Étape 1 : Vérifier si le badge existe et est actif
        const char *param_values[1] = {badge_id_str};
        PGresult *res = PQexecParams(
            db_conn,
            "SELECT actif, date_expiration FROM Badge WHERE id_badge = $1",
            1,
            NULL,
            param_values,
            NULL,
            NULL,
            0);

        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            fprintf(stderr, "Erreur lors de la vérification du badge : %s\n",
                    PQerrorMessage(db_conn));
            PQclear(res);
            response[0] = ACCESS_DENIED;
            const char *message = "Erreur du serveur.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);
            return;
        }

        if (PQntuples(res) == 0) {
            // Badge inexistant
            PQclear(res);
            response[0] = ACCESS_DENIED;
            const char *message = "Badge inexistant.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);

            // Insérer l'événement dans l'historique avec résultat négatif
            insert_historique(db_conn, badge_id_str, client_id, 0);

            return;
        }

        char *badge_actif_str = PQgetvalue(res, 0, 0);
        bool badge_actif = (strcmp(badge_actif_str, "t") == 0);

        char *date_expiration_str = PQgetvalue(res, 0, 1);
        PQclear(res);

        // Vérifier si le badge est actif
        if (!badge_actif) {
            response[0] = ACCESS_DENIED;
            const char *message = "Badge inactif.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);

            // Insérer l'événement dans l'historique avec résultat négatif
            insert_historique(db_conn, badge_id_str, client_id, 0);

            return;
        }

        // Vérifier si le badge est expiré
        time_t now = time(NULL);
        struct tm tm_expiration = {0};
        strptime(date_expiration_str, "%Y-%m-%d", &tm_expiration);
        time_t expiration_time = mktime(&tm_expiration);

        if (difftime(expiration_time, now) < 0) {
            response[0] = ACCESS_DENIED;
            const char *message = "Badge expiré.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);

            // Insérer l'événement dans l'historique avec résultat négatif
            insert_historique(db_conn, badge_id_str, client_id, 0);

            return;
        }

        // Étape 2 : Récupérer l'individu associé au badge
        res = PQexecParams(
            db_conn,
            "SELECT i.id_individu, r.niveau_acces "
            "FROM Individu i "
            "JOIN Role r ON i.id_role = r.id_role "
            "WHERE i.id_badge = $1",
            1,
            NULL,
            param_values,
            NULL,
            NULL,
            0);

        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            fprintf(stderr, "Erreur lors de la récupération des informations de l'individu : %s\n",
                    PQerrorMessage(db_conn));
            PQclear(res);
            response[0] = ACCESS_DENIED;
            const char *message = "Erreur du serveur.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);
            return;
        }

        if (PQntuples(res) == 0) {
            // Aucun individu associé à ce badge
            PQclear(res);
            response[0] = ACCESS_DENIED;
            const char *message = "Aucun individu associé à ce badge.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);

            // Insérer l'événement dans l'historique avec résultat négatif
            insert_historique(db_conn, badge_id_str, client_id, 0);

            return;
        }

        char *id_individu_str = PQgetvalue(res, 0, 0);
        int niveau_acces_individu = atoi(PQgetvalue(res, 0, 1));
        PQclear(res);

        // Étape 3 : Récupérer le niveau d'accès de la porte
        char client_id_str[16];
        snprintf(client_id_str, sizeof(client_id_str), "%u", client_id);

        const char *param_values2[1] = {client_id_str};
        res = PQexecParams(
            db_conn,
            "SELECT niveau_acces FROM Porte WHERE id_porte = $1",
            1,
            NULL,
            param_values2,
            NULL,
            NULL,
            0);

        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            fprintf(stderr, "Erreur lors de la récupération des informations de la porte : %s\n",
                    PQerrorMessage(db_conn));
            PQclear(res);
            response[0] = ACCESS_DENIED;
            const char *message = "Erreur du serveur.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);
            return;
        }

        if (PQntuples(res) == 0) {
            // Aucune porte trouvée avec cet ID
            PQclear(res);
            response[0] = ACCESS_DENIED;
            const char *message = "Porte inconnue.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);

            // Insérer l'événement dans l'historique avec résultat négatif
            insert_historique(db_conn, badge_id_str, client_id, 0);

            return;
        }

        int niveau_acces_porte = atoi(PQgetvalue(res, 0, 0));
        PQclear(res);

        // Étape 4 : Comparer les niveaux d'accès
        if (niveau_acces_individu >= niveau_acces_porte) {
            // Vérifier si la porte est dans une zone sécurisée
            // Obtenir l'id_espace à partir de la porte
            const char *param_values3[1] = {client_id_str};
            res = PQexecParams(
                db_conn,
                "SELECT id_espace FROM Espace WHERE id_porte = $1",
                1,
                NULL,
                param_values3,
                NULL,
                NULL,
                0);

            if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                fprintf(stderr, "Erreur lors de la récupération de l'espace : %s\n",
                        PQerrorMessage(db_conn));
                PQclear(res);
                response[0] = ACCESS_DENIED;
                const char *message = "Erreur du serveur.";
                strcpy((char *)&response[1], message);
                *response_length = 1 + strlen(message);
                return;
            }

            if (PQntuples(res) == 0) {
                // Aucun espace trouvé pour cette porte, on considère qu'il n'y a pas de zone sécurisée
                PQclear(res);
                // Accès autorisé
                response[0] = ACCESS_GRANTED;
                const char *message = "Accès autorisé.";
                strcpy((char *)&response[1], message);
                *response_length = 1 + strlen(message);

                // Insérer l'événement dans l'historique avec résultat positif
                insert_historique(db_conn, badge_id_str, client_id, 1);
                return;
            }

            char *id_espace_str = PQgetvalue(res, 0, 0);
            PQclear(res);

            // Vérifier si l'espace est dans une zone sécurisée
            const char *param_values4[1] = {id_espace_str};
            res = PQexecParams(
                db_conn,
                "SELECT zs.id_zone_securise, zs.niveau_securite "
                "FROM Se_Trouver_Dans std "
                "JOIN Zone_Securise zs ON std.id_zone_securise = zs.id_zone_securise "
                "WHERE std.id_espace = $1",
                1,
                NULL,
                param_values4,
                NULL,
                NULL,
                0);

            if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                fprintf(stderr, "Erreur lors de la vérification de la zone sécurisée : %s\n",
                        PQerrorMessage(db_conn));
                PQclear(res);
                response[0] = ACCESS_DENIED;
                const char *message = "Erreur du serveur.";
                strcpy((char *)&response[1], message);
                *response_length = 1 + strlen(message);
                return;
            }

            if (PQntuples(res) == 0) {
                // L'espace n'est pas dans une zone sécurisée
                PQclear(res);
                // Accès autorisé
                response[0] = ACCESS_GRANTED;
                const char *message = "Accès autorisé.";
                strcpy((char *)&response[1], message);
                *response_length = 1 + strlen(message);

                // Insérer l'événement dans l'historique avec résultat positif
                insert_historique(db_conn, badge_id_str, client_id, 1);
                return;
            }

            // Si on arrive ici, la porte est dans une zone sécurisée
            // On peut définir que toute zone sécurisée nécessite un mot de passe
            PQclear(res);

            // Envoyer une réponse indiquant que le mot de passe est requis
            response[0] = PASSWORD_REQUIRED;
            const char *message = "Mot de passe requis.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);

            // Vous pouvez enregistrer l'état si nécessaire

        } else {
            // Accès refusé
            response[0] = ACCESS_DENIED;
            const char *message = "Accès refusé : niveau d'accès insuffisant.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);

            // Insérer l'événement dans l'historique avec résultat négatif
            insert_historique(db_conn, badge_id_str, client_id, 0);
        }
        break;
    }
    case REQUEST_SEND_PASSWORD: {
        // Le client envoie le mot de passe (salaire)
        // Extraction des données (ID du badge et mot de passe)
        if (data_length < sizeof(uint32_t) + 1) {
            fprintf(stderr, "Données insuffisantes pour le mot de passe.\n");
            response[0] = ACCESS_DENIED;
            const char *message = "Données insuffisantes pour le mot de passe.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);
            return;
        }

        uint32_t badge_id_network_order;
        memcpy(&badge_id_network_order, &request[8], sizeof(uint32_t));
        uint32_t badge_id = ntohl(badge_id_network_order);

        char badge_id_str[16];
        snprintf(badge_id_str, sizeof(badge_id_str), "%u", badge_id);

        // Le mot de passe commence après l'ID du badge
        size_t password_length = data_length - sizeof(uint32_t);
        char password_str[256];
        memcpy(password_str, &request[8 + sizeof(uint32_t)], password_length);
        password_str[password_length] = '\0';

        // Récupérer le salaire de l'employé associé au badge
        const char *param_values[1] = {badge_id_str};
        PGresult *res = PQexecParams(
            db_conn,
            "SELECT e.salaire "
            "FROM Employe e "
            "JOIN Individu i ON e.id_employe = i.id_individu "
            "WHERE i.id_badge = $1",
            1,
            NULL,
            param_values,
            NULL,
            NULL,
            0);

        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            fprintf(stderr, "Erreur lors de la récupération du salaire : %s\n",
                    PQerrorMessage(db_conn));
            PQclear(res);
            response[0] = ACCESS_DENIED;
            const char *message = "Erreur du serveur.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);
            return;
        }

        if (PQntuples(res) == 0) {
            // Aucun employé trouvé pour ce badge
            PQclear(res);
            response[0] = ACCESS_DENIED;
            const char *message = "Aucun employé associé à ce badge.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);
            return;
        }

        char *salaire_str = PQgetvalue(res, 0, 0);
        PQclear(res);

        // Comparer le mot de passe saisi avec le salaire
        if (strcmp(password_str, salaire_str) == 0) {
            // Mot de passe correct, accès autorisé
            response[0] = ACCESS_GRANTED;
            const char *message = "Accès autorisé.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);

            // Insérer l'événement dans l'historique avec résultat positif
            insert_historique(db_conn, badge_id_str, client_id, 1);
        } else {
            // Mot de passe incorrect, accès refusé
            response[0] = ACCESS_DENIED;
            const char *message = "Mot de passe incorrect.";
            strcpy((char *)&response[1], message);
            *response_length = 1 + strlen(message);

            // Insérer l'événement dans l'historique avec résultat négatif
            insert_historique(db_conn, badge_id_str, client_id, 0);
        }

        break;
    }
    case REQUEST_MAINTENANCE_SIGNAL: {
        // Le client envoie un signal de maintenance
        printf("Signal de maintenance reçu de la porte ID %u\n", client_id);

        // Insérer le résultat de maintenance
        char client_id_str[16];
        snprintf(client_id_str, sizeof(client_id_str), "%u", client_id);

        insert_maintenance_result(db_conn, client_id_str, true);

        // Pas besoin d'envoyer une réponse
        *response_length = 0;
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

int initialize_server(ServerContext *context) {
    // Création du socket serveur
    context->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (check(context->server_socket, "Erreur de création du socket serveur") == -1) {
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
        return -1;
    }

    // Mise en écoute
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

void *handle_client_thread(void *args) {
    ClientThreadArgs *client_args = (ClientThreadArgs *)args;
    int client_socket = client_args->client_socket;
    struct sockaddr_in client_addr = client_args->client_addr;
    free(client_args);  // Libérer la mémoire allouée pour les arguments

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

    uint8_t buffer[BUFFER_SIZE];
    uint8_t response[BUFFER_SIZE];
    size_t response_length = 0;

    // Régler le timeout du socket
    struct timeval timeout;
    timeout.tv_sec = CLIENT_TIMEOUT_SEC;
    timeout.tv_usec = 0;

    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                   sizeof(timeout)) < 0) {
        perror("Erreur lors du réglage du timeout du socket");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Chaque thread a besoin de sa propre connexion à la base de données
    PGconn *db_conn = connect_db();
    if (db_conn == NULL) {
        fprintf(stderr, "Impossible de se connecter à la base de données.\n");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Recevoir les données du client
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

    // Traiter la requête du client
    process_client_request(db_conn, buffer, (size_t)bytes_received, response,
                           &response_length, client_ip);

    // Envoyer la réponse au client si nécessaire
    if (response_length > 0) {
        ssize_t bytes_sent = send(client_socket, response, response_length, 0);
        if (bytes_sent < 0) {
            perror("Erreur d'envoi de la réponse au client");
        }
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
