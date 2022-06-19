# Proyecto_Protos

Por ahora, tenemos un servidor que acepta multiples conexiones y lee lo que envia cada cliente mediante un select.
El servidor le reenvía al cliente lo que él mismo envió.


## Instrucciones para probarlo

Primero que nada se deben compilar los archivos
```
make all
```

Después se debe correr el servidor:
```
./bin/server -u user:pass
```

Si se quiere aprovechar la opción de management se debe ejecutar su cliente:
```
./bin/client -u user:pass
```

Para correr los tests:
```
./testing.sh user:pass <opcion>
```
Dentro de las opciones se encuentran:

- netstat (1.1 y 1.2)
- integrity-velocity (1.3)
- multi-connection (1.4)
- invalid-ipv4-curl (1.7)
- invalid-ipv6-curl (1.8)
- invalid_dns-curl (1.9)
- dns-ipv6 (1.10)
- multi-ip (1.11)
- http-proxy (1.13)
