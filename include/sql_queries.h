#ifndef SQL_QUERIES_H
#define SQL_QUERIES_H

// Création de la table Badge
static const char* CREATE_TABLE_BADGE =
    "CREATE TABLE Badge ("
    "    id_badge SERIAL PRIMARY KEY NOT NULL,"
    "    actif BOOLEAN NOT NULL,"
    "    date_creation DATE NOT NULL CHECK (date_creation BETWEEN '2024-01-01' AND CURRENT_DATE),"
    "    date_expiration DATE NOT NULL CHECK (date_expiration >= '2024-01-01')"
    ");";

// Création de la table Role
static const char* CREATE_TABLE_ROLE =
    "CREATE TABLE Role ("
    "    id_role SERIAL PRIMARY KEY NOT NULL,"
    "    nom_role VARCHAR(30) NOT NULL,"
    "    niveau_acces INTEGER NOT NULL CHECK (niveau_acces BETWEEN 1 AND 5),"
    "    salaire_min INTEGER NOT NULL CHECK (salaire_min > 0),"
    "    salaire_max INTEGER NOT NULL CHECK (salaire_max > salaire_min),"
    "    salaire INTEGER NOT NULL CHECK (salaire BETWEEN salaire_min AND salaire_max)"
    ");";

// Création de la table Individu
static const char* CREATE_TABLE_INDIVIDU =
    "CREATE TABLE Individu ("
    "    id_individu SERIAL PRIMARY KEY NOT NULL,"
    "    nom_individu VARCHAR(30) NOT NULL,"
    "    prenom_individu VARCHAR(30) NOT NULL,"
    "    tel VARCHAR(15) UNIQUE,"
    "    email VARCHAR(100) NOT NULL,"
    "    id_role INTEGER NOT NULL REFERENCES Role(id_role),"
    "    id_badge INTEGER NOT NULL REFERENCES Badge(id_badge)"
    ");";

// Création de la table Porte
static const char* CREATE_TABLE_PORTE =
    "CREATE TABLE Porte ("
    "    id_porte CHAR(5) PRIMARY KEY NOT NULL,"
    "    descriptif_acces VARCHAR(100) NOT NULL,"
    "    niveau_acces INTEGER NOT NULL CHECK (niveau_acces BETWEEN 1 AND 5)"
    ");";

// Création de la table Historique
static const char* CREATE_TABLE_HISTORIQUE =
    "CREATE TABLE Historique ("
    "    id_historique SERIAL PRIMARY KEY NOT NULL,"
    "    resultat INTEGER NOT NULL,"
    "    dateheure TIMESTAMP NOT NULL CHECK (dateheure <= CURRENT_TIMESTAMP),"
    "    id_badge INTEGER NOT NULL REFERENCES Badge(id_badge),"
    "    id_porte CHAR(5) NOT NULL REFERENCES Porte(id_porte)"
    ");";

// Création de la table Employe
static const char* CREATE_TABLE_EMPLOYE =
    "CREATE TABLE Employe ("
    "    id_employe INTEGER PRIMARY KEY NOT NULL REFERENCES Individu(id_individu),"
    "    date_embauche DATE NOT NULL CHECK (date_embauche BETWEEN '2024-01-01' AND CURRENT_DATE),"
    "    date_fin_contrat DATE CHECK (date_fin_contrat IS NULL OR date_fin_contrat > '2024-01-01'),"
    "    type_contrat VARCHAR(20) NOT NULL"
    ");";

// Création de la table Visiteur
static const char* CREATE_TABLE_VISITEUR =
    "CREATE TABLE Visiteur ("
    "    id_visiteur INTEGER PRIMARY KEY NOT NULL REFERENCES Individu(id_individu),"
    "    niveau_acces INTEGER NOT NULL CHECK (niveau_acces BETWEEN 1 AND 5),"
    "    date_visite DATE NOT NULL CHECK (date_visite > '2024-01-01'),"
    "    societe VARCHAR(50) NOT NULL"
    ");";

// Création de la table Maintenance
static const char* CREATE_TABLE_MAINTENANCE =
    "CREATE TABLE Maintenance ("
    "    id_maintenance SERIAL PRIMARY KEY NOT NULL,"
    "    resultat BOOLEAN NOT NULL,"
    "    id_porte CHAR(5) NOT NULL REFERENCES Porte(id_porte)"
    ");";

// Création de la table Espace
static const char* CREATE_TABLE_ESPACE =
    "CREATE TABLE Espace ("
    "    id_espace SERIAL PRIMARY KEY NOT NULL,"
    "    nom_espace VARCHAR(50) NOT NULL,"
    "    superficie VARCHAR(10) CHECK (superficie IS NULL OR CAST(superficie AS INTEGER) < 100000),"
    "    id_porte CHAR(5) NOT NULL REFERENCES Porte(id_porte)"
    ");";

// Création de la table Zone_Securise
static const char* CREATE_TABLE_ZONE_SECURISE =
    "CREATE TABLE Zone_Securise ("
    "    id_zone_securise SERIAL PRIMARY KEY NOT NULL,"
    "    niveau_securite INTEGER NOT NULL CHECK (niveau_securite BETWEEN 1 AND 5)"
    ");";

// Création de la table Se_Trouver_Dans
static const char* CREATE_TABLE_SE_TROUVER_DANS =
    "CREATE TABLE Se_Trouver_Dans ("
    "    id_zone_securise INTEGER NOT NULL REFERENCES Zone_Securise(id_zone_securise),"
    "    id_espace INTEGER NOT NULL REFERENCES Espace(id_espace),"
    "    PRIMARY KEY (id_zone_securise, id_espace)"
    ");";

#endif // SQL_QUERIES_H
