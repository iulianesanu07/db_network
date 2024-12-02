import socket
import struct

SERVER_IP = '172.20.10.7'  # Remplacez par l'adresse IP de votre serveur
SERVER_PORT = 8080

# Création de la classe Card
class Card:
    def __init__(self, card_id):
        self.id = card_id

# Création de la carte
id_card_in_user = input("Entrer l'ID de la carte : ")
card = Card(id_card_in_user)

# Création du socket
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

try:
    # Connexion au serveur
    server_addr = (SERVER_IP, SERVER_PORT)
    client_socket.connect(server_addr)

    # Préparation du message selon le nouveau protocole
    protocol_version = 1  # Version du protocole
    request_type = 1      # Type de requête (1 pour vérification de badge)
    client_id = 12345     # Identifiant client (à personnaliser si nécessaire)
    data = card.id.encode('utf-8')
    data_length = len(data)  # Taille des données (identifiant client + données)

    # Construction du message
    # '!' pour spécifier l'ordre des octets réseau (big-endian)
    # 'B' pour un entier non signé d'1 octet (version, type)
    # 'H' pour un entier non signé de 2 octets (taille)
    # 'I' pour un entier non signé de 4 octets (identifiant client)
    header = struct.pack('!BBHI', protocol_version, request_type, data_length, client_id)
    message = header + data

    # Envoi du message
    client_socket.sendall(message)

    # Réception de la réponse du serveur
    response = client_socket.recv(1024)

    # Traitement de la réponse
    if response:
        # Le premier octet est le code de réponse
        code = response[0]
        message = response[1:].decode('utf-8')
        if code == 1:
            print(message)
        else:
            print(message)
    else:
        print("Aucune réponse du serveur.")

finally:
    # Fermeture de la connexion
    client_socket.close()
