#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>

int main() {
    PGconn *conn;

    // Paramètres de connexion à adapter
    const char *conninfo = "host=localhost port=5432 user=iulianesanu";

    // Create a connection
    conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Error while connecting to the database : %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        exit(EXIT_FAILURE);
    }

    // Nice t'es connecte  
    printf("Connection Established\n");
    printf("Port: %s\n", PQport(conn));
    printf("Host: %s\n", PQhost(conn));
    printf("DBName: %s\n", PQdb(conn));

    // Test Creation table etu
    char *query = "CREATE TABLE etu_bis2 ( "\
                  " nom VARCHAR(100), "\
                  " num int);";
    
    // Submit the query and retrieve the result
    PGresult *res = PQexec(conn, query);

    // Check the status of the query result
    ExecStatusType resStatus = PQresultStatus(res);

    // Convert the status to a string and print it
    printf("Query Status: %s\n", PQresStatus(resStatus));

    // Check if the query execution was successful 
    if (resStatus != PGRES_COMMAND_OK) {
      printf("Error while executing the query: %s\n", PQerrorMessage(conn));
      PQclear(res);         // Clear the result
      PQfinish(conn);
      exit(EXIT_FAILURE);
    }

    // We have successfully executed the query
    printf("Querry Executed Successfully\n");

    // Get the number of rows and columns in the query result
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

    // Clear the result
    PQclear(res);

    PQfinish(conn);

    return EXIT_SUCCESS;
} 
