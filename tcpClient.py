import socket
import struct
import threading

# Configuration du client
SERVER_IP = '172.20.10.7'  # Adresse IP du serveur (à adapter)
SERVER_PORT = 8080         # Port du serveur

CLIENT_HOST = '0.0.0.0'    # Écoute sur toutes les interfaces
CLIENT_PORT = 9090         # Port d'écoute pour les requêtes du serveur

PROTOCOL_VERSION = 1

# Codes de réponse
ACCESS_DENIED = 0
ACCESS_GRANTED = 1

# Types de requêtes
REQUEST_CHECK_BADGE = 1
REQUEST_MAINTENANCE_CHECK = 2

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
    data = badge_id.encode('utf-8')
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
        print(f"Accès autorisé : {message}")
    elif response_code == ACCESS_DENIED:
        print(f"Accès refusé : {message}")
    else:
        print(f"Code de réponse inconnu : {response_code}")

    sock.close()

def handle_server_connection(conn, addr):
    try:
        # Recevoir la requête du serveur
        data = conn.recv(1024)
        if not data:
            return

        # Traiter la requête
        version = data[0]
        request_type = data[1]

        if version != PROTOCOL_VERSION:
            print("Version de protocole incompatible")
            return

        if request_type == REQUEST_MAINTENANCE_CHECK:
            # Répondre au serveur pour indiquer que la porte est en ligne
            response_code = ACCESS_GRANTED  # La porte est en ligne
            message = "Porte en ligne"
            response = struct.pack('!B', response_code) + message.encode('utf-8')
            conn.sendall(response)
            print(f"Réponse à la requête de maintenance envoyée au serveur depuis {CLIENT_HOST}:{CLIENT_PORT}")
        else:
            print(f"Type de requête inconnu : {request_type}")

    except Exception as e:
        print(f"Erreur lors du traitement de la requête de maintenance : {e}")
    finally:
        conn.close()

def start_client_server():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((CLIENT_HOST, CLIENT_PORT))
    server_socket.listen(5)
    print(f"Client en écoute pour les requêtes de maintenance sur le port {CLIENT_PORT}")

    while True:
        conn, addr = server_socket.accept()
        threading.Thread(target=handle_server_connection, args=(conn, addr), daemon=True).start()

def main():
    # Démarrer le serveur du client dans un thread séparé
    threading.Thread(target=start_client_server, daemon=True).start()

    # Simuler l'envoi de requêtes de vérification de badge
    while True:
        print("\n--- Vérification de badge ---")
        badge_id = input("Entrez l'ID du badge à vérifier (ou 'exit' pour quitter) : ")
        if badge_id.lower() == 'exit':
            break
        client_id = int(input("Entrez l'ID du client : "))
        send_badge_verification(badge_id, client_id)

if __name__ == '__main__':
    main()
