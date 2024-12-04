import psycopg2

DB_HOST = 'localhost'
DB_PORT = '5432'
DB_USER = 'iulianesanu'
DB_PASSWORD = "Let's_go" 
DB_NAME = 'db_reseau'

def connect_db():
    try:
        conn = psycopg2.connect(
            host=DB_HOST,
            port=DB_PORT,
            dbname=DB_NAME,
            user=DB_USER,
            password=DB_PASSWORD
        )
        return conn
    except Exception as e:
        print(f"Erreur de connexion à la base de données : {e}")
        return None

def create_role(conn):
    nom_role = input("Nom du rôle : ")
    niveau_acces = int(input("Niveau d'accès (1-10) : "))
    salaire_min = int(input("Salaire minimum : "))
    salaire_max = int(input("Salaire maximum : "))

    cur = conn.cursor()
    cur.execute("""
        INSERT INTO Role (nom_role, niveau_acces, salaire_min, salaire_max)
        VALUES (%s, %s, %s, %s) RETURNING id_role;
    """, (nom_role, niveau_acces, salaire_min, salaire_max))
    id_role = cur.fetchone()[0]
    conn.commit()
    print(f"Rôle créé avec ID {id_role}")

def create_badge(conn):
    actif = True
    date_creation = input("Date de création (YYYY-MM-DD) : ")
    date_expiration = input("Date d'expiration (YYYY-MM-DD) : ")

    cur = conn.cursor()
    cur.execute("""
        INSERT INTO Badge (actif, date_creation, date_expiration)
        VALUES (%s, %s, %s) RETURNING id_badge;
    """, (actif, date_creation, date_expiration))
    id_badge = cur.fetchone()[0]
    conn.commit()
    print(f"Badge créé avec ID {id_badge}")

def create_individu(conn):
    nom = input("Nom de l'individu : ")
    prenom = input("Prénom de l'individu : ")
    tel = input("Téléphone : ")
    email = input("Email : ")
    id_role = int(input("ID du rôle : "))
    id_badge = int(input("ID du badge : "))

    cur = conn.cursor()
    cur.execute("""
        INSERT INTO Individu (nom_individu, prenom_individu, tel, email, id_role, id_badge)
        VALUES (%s, %s, %s, %s, %s, %s) RETURNING id_individu;
    """, (nom, prenom, tel, email, id_role, id_badge))
    id_individu = cur.fetchone()[0]
    conn.commit()
    print(f"Individu créé avec ID {id_individu}")

def create_employe(conn):
    id_individu = int(input("ID de l'individu : "))
    date_embauche = input("Date d'embauche (YYYY-MM-DD) : ")
    date_fin_contrat = input("Date de fin de contrat (YYYY-MM-DD ou vide) : ")
    if date_fin_contrat == '':
        date_fin_contrat = None
    type_contrat = input("Type de contrat : ")
    salaire = int(input("Salaire : "))

    cur = conn.cursor()
    cur.execute("""
        INSERT INTO Employe (id_employe, date_embauche, date_fin_contrat, type_contrat, salaire)
        VALUES (%s, %s, %s, %s, %s);
    """, (id_individu, date_embauche, date_fin_contrat, type_contrat, salaire))
    conn.commit()
    print(f"Employé créé pour l'individu ID {id_individu}")

def create_porte(conn):
    id_porte = int(input("ID de la porte : "))
    descriptif = input("Descriptif de l'accès : ")
    niveau_acces = int(input("Niveau d'accès (1-10) : "))

    cur = conn.cursor()
    cur.execute("""
        INSERT INTO Porte (id_porte, descriptif_acces, niveau_acces)
        VALUES (%s, %s, %s);
    """, (id_porte, descriptif, niveau_acces))
    conn.commit()
    print(f"Porte créée avec ID {id_porte}")

def create_zone_securise(conn):
    niveau_securite = int(input("Niveau de sécurité (1-10) : "))

    cur = conn.cursor()
    cur.execute("""
        INSERT INTO Zone_Securise (niveau_securite)
        VALUES (%s) RETURNING id_zone_securise;
    """, (niveau_securite,))
    id_zone = cur.fetchone()[0]
    conn.commit()
    print(f"Zone sécurisée créée avec ID {id_zone}")

def create_espace(conn):
    nom_espace = input("Nom de l'espace : ")
    superficie = input("Superficie (nombre ou vide) : ")
    if superficie == '':
        superficie = None
    else:
        superficie = int(superficie)
    id_porte = int(input("ID de la porte associée : "))

    cur = conn.cursor()
    cur.execute("""
        INSERT INTO Espace (nom_espace, superficie, id_porte)
        VALUES (%s, %s, %s) RETURNING id_espace;
    """, (nom_espace, superficie, id_porte))
    id_espace = cur.fetchone()[0]
    conn.commit()
    print(f"Espace créé avec ID {id_espace}")

def link_espace_zone(conn):
    id_zone = int(input("ID de la zone sécurisée : "))
    id_espace = int(input("ID de l'espace : "))

    cur = conn.cursor()
    cur.execute("""
        INSERT INTO Se_Trouver_Dans (id_zone_securise, id_espace)
        VALUES (%s, %s);
    """, (id_zone, id_espace))
    conn.commit()
    print(f"Espace ID {id_espace} lié à la zone sécurisée ID {id_zone}")

def main():
    conn = connect_db()
    if conn is None:
        return

    actions = {
        '1': ('Créer un rôle', create_role),
        '2': ('Créer un badge', create_badge),
        '3': ('Créer un individu', create_individu),
        '4': ('Créer un employé', create_employe),
        '5': ('Créer une porte', create_porte),
        '6': ('Créer une zone sécurisée', create_zone_securise),
        '7': ('Créer un espace', create_espace),
        '8': ('Lier un espace à une zone sécurisée', link_espace_zone),
        '9': ('Quitter', None)
    }

    while True:
        print("\n--- Menu ---")
        for key, (desc, _) in actions.items():
            print(f"{key}. {desc}")

        choice = input("Choisissez une action : ")

        if choice == '9':
            print("Au revoir!")
            break
        elif choice in actions:
            action = actions[choice][1]
            action(conn)
        else:
            print("Choix invalide.")

    conn.close()

if __name__ == '__main__':
    main()
