# client.py

import socket
import struct
import threading
import time

# Configuration du client
SERVER_IP = '172.20.10.7'  # Adresse IP du serveur (à adapter)
SERVER_PORT = 8080       # Port du serveur

PROTOCOL_VERSION = 1

# Codes de réponse
ACCESS_DENIED = 0
ACCESS_GRANTED = 1
PASSWORD_REQUIRED = 2

# Types de requêtes
REQUEST_CHECK_BADGE = 1
REQUEST_MAINTENANCE_SIGNAL = 2
REQUEST_SEND_PASSWORD = 3

def send_badge_verification(badge_id, client_id):
    # Création du socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((SERVER_IP, SERVER_PORT))
    except Exception as e:
        print(f"Erreur de connexion au serveur : {e}")
        return

    # Préparation de la requête
    version = PROTOCOL_VERSION
    request_type = REQUEST_CHECK_BADGE
    data = str(badge_id).encode('utf-8')
    data_length = len(data)

    # Construction de l'en-tête
    header = struct.pack('!BBH', version, request_type, data_length)
    header += struct.pack('!I', client_id)

    # Envoi de la requête
    try:
        sock.sendall(header + data)
    except Exception as e:
        print(f"Erreur lors de l'envoi de la requête : {e}")
        sock.close()
        return

    # Réception de la réponse
    try:
        response = sock.recv(1024)
    except Exception as e:
        print(f"Erreur lors de la réception de la réponse : {e}")
        sock.close()
        return

    if not response:
        print("Aucune réponse du serveur.")
        sock.close()
        return

    # Traitement de la réponse
    response_code = response[0]
    message = response[1:].decode('utf-8')

    if response_code == ACCESS_GRANTED:
        print(f"-> {message}")
    elif response_code == ACCESS_DENIED:
        print(f"-> {message}")
    elif response_code == PASSWORD_REQUIRED:
        print(f"-> {message}")
        # Demander le mot de passe à l'utilisateur
        password = input("Veuillez entrer votre mot de passe : ")

        # Envoyer le mot de passe au serveur
        send_password(badge_id, client_id, password)
    else:
        print(f"Code de réponse inconnu : {response_code}")

    sock.close()

def send_password(badge_id, client_id, password):
    # Création du socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((SERVER_IP, SERVER_PORT))
    except Exception as e:
        print(f"Erreur de connexion au serveur : {e}")
        return

    # Préparation de la requête
    version = PROTOCOL_VERSION
    request_type = REQUEST_SEND_PASSWORD

    # Les données contiennent l'ID du badge (entier) et le mot de passe (chaîne)
    badge_id_bytes = struct.pack('!I', badge_id)
    password_bytes = password.encode('utf-8')
    data = badge_id_bytes + password_bytes
    data_length = len(data)

    # Construction de l'en-tête
    header = struct.pack('!BBH', version, request_type, data_length)
    header += struct.pack('!I', client_id)

    # Envoi de la requête
    try:
        sock.sendall(header + data)
    except Exception as e:
        print(f"Erreur lors de l'envoi du mot de passe : {e}")
        sock.close()
        return

    # Réception de la réponse
    try:
        response = sock.recv(1024)
    except Exception as e:
        print(f"Erreur lors de la réception de la réponse : {e}")
        sock.close()
        return

    if not response:
        print("Aucune réponse du serveur.")
        sock.close()
        return

    # Traitement de la réponse
    response_code = response[0]
    message = response[1:].decode('utf-8')

    if response_code == ACCESS_GRANTED:
        print(f"-> {message}")
    elif response_code == ACCESS_DENIED:
        print(f"-> {message}")
    else:
        print(f"Code de réponse inconnu : {response_code}")

    sock.close()

def send_maintenance_signal(client_id):
    while True:
        # Attendre un certain intervalle avant d'envoyer le signal
        time.sleep(10)  # Par exemple, toutes les 10 secondes

        # Création du socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.connect((SERVER_IP, SERVER_PORT))
        except Exception:
            # Ne pas afficher d'erreur pour ne pas interférer avec l'utilisateur
            continue

        # Préparation de la requête
        version = PROTOCOL_VERSION
        request_type = REQUEST_MAINTENANCE_SIGNAL
        data = b''  # Pas de données supplémentaires
        data_length = len(data)

        # Construction de l'en-tête
        header = struct.pack('!BBH', version, request_type, data_length)
        header += struct.pack('!I', client_id)

        # Envoi de la requête
        try:
            sock.sendall(header + data)
        except Exception:
            sock.close()
            continue

        # Pas besoin de recevoir de réponse
        sock.close()

def main():
    # Obtenir l'ID du client (porte)
    client_id = int(input("Entrez l'ID du client (porte) : "))

    # Démarrer le thread pour envoyer les signaux de maintenance
    threading.Thread(target=send_maintenance_signal, args=(client_id,), daemon=True).start()

    # Boucle principale pour la vérification de badge
    while True:
        badge_id_input = input("\nEntrez l'ID du badge à vérifier (ou 'exit' pour quitter) : ")
        if badge_id_input.lower() == 'exit':
            break
        try:
            badge_id = int(badge_id_input)
        except ValueError:
            print("Veuillez entrer un ID de badge valide (entier).")
            continue
        send_badge_verification(badge_id, client_id)

if __name__ == '__main__':
    main()
