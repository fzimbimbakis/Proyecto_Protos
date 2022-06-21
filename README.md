# Servidor proxy SOCKSv5 [RFC1928] 

## Ubicacion de los Materiales 🧭
Archivos fuente .c se encuentran dentro de las subcarpetas
```
./src/
```
Includes necesarios para el funcionamiento
```
./include/
```
Archivos ejecutables server y client
```
./bin/
```

##Instrucciones de Compilacion 🛠️
Para compilar todo el proyecto
```
make all
```
Para compilar solo el servidor
```
make server
```
Para compilar solo el cliente
```
make client
```
Si se quiere realizar una limpieza de archivos objeto
```
make clean
```

## Ejecución 🚀
Para correr el server
```
./bin/server -u <user:pass>
```
También tiene más flags disponibles para ejecutarlo.
Se debe mirar su respectivo manual
```
man ./socksd.8
```

Si se quiere aprovechar la opción de management se debe ejecutar su cliente. 
Solo se acepta el usuario y contraseña <ins>admin:admin</ins> indicado en credentials.txt
```
./bin/client -u admin:admin
```
También tiene más flags disponibles para ejecutarlo.
Se debe mirar su respectivo manual
```
man ./clientM16.8
```

## Testing 👌
Para correr los tests:
```
./testing.sh <user:pass> <port> <opcion>
```
Dentro de las opciones se encuentran:

- netstat 
  - (<ins>1.1</ins>)  Defaults bindings 
  - (<ins>1.2</ins>)  Cambio de bindings
- integrity-velocity
  - (<ins>1.3</ins>)  Proxy una conexión por vez. Verificar integridad y velocidad
- multi-connection 
  - (<ins>1.4</ins>)  Prueba de múltiples conexiones simultáneas (ejercutar en diferentes terminales)
- curl
  - (<ins>1.5</ins>)  Desconexión repentina cliente. Durante la transferencia matar curl
  - (<ins>1.6</ins>)  Desconexion repentina server. Durante la transferencia matar server
- invalid-ipv4-curl
  - (<ins>1.7</ins>)  Origin server (IPV4) no presta servicio
- invalid-ipv6-curl
  - (<ins>1.8</ins>)  Origin server (IPV6) no presta servicio
- invalid-dns-curl 
  - (<ins>1.9</ins>)  Falla resolución de nombres
- dns-ipv6 
  - (<ins>1.10</ins>)  Comportamiento origin server resuelve DNS IPV6
- multi-ip 
  - (<ins>1.11</ins>) Origin server con múltiples direcciones IP (una falla)
- http-proxy 
  - (<ins>1.13</ins>)  Probar enviarle http




## Autores 💭
* **Gaspar Budó Berra**
* **Bruno Squillari**
* **Facundo Zimbimbakis**
