from socket import *
"""
# Creation du socket
client_socket = socket(AF_INET, SOCK_STREAM)

# Adresse IP du serveur et port
server_addr = ('localhost', 8080)

# Connexion au serveur
client_socket.connect(server_addr)

# Envoi de donnees au serveur
msg = "Trop balese".encode('utf-8')
client_socket.sendall(msg)

# Reception des donnees du serveur
data = client_socket.recv(1024)
print(data.decode('utf-8'))

# Fermeture de la connexion
client_socket.close()
"""

SERVER_ID = '192.168.1.36'
SERVER_PORT = 8080

# Definition de la classe Card (en theorie temporaire)
class Card:
    def __init__(self,id):
        self.id = id 

# Creation carte
idCardInUser = 0
idCardInUser = int(input("Entrer l'id de la carte : "))
card1 = Card(idCardInUser)

# Mise en place du serveur
socket = socket(AF_INET, SOCK_STREAM)
server_addr = (SERVER_ID, SERVER_PORT)
socket.connect(server_addr)

# Envoi du message
msg = ("ID Card : " + str(card1.id)).encode('utf-8')
socket.sendall(msg)

# Reception des donnees du serveur
data = socket.recv(1024)
print(data.decode('utf-8'))

# Fermeture de la connexion
socket.close()

print("Fin programme")
