# Proyecto_Protos

Por ahora, tenemos un servidor que acepta multiples conexiones y lee lo que envia cada cliente mediante un select.
El servidor le reenvía al cliente lo que él mismo envió.


## Instrucciones para probarlo

Primero que nada se deben compilar los archivos
```
make all
```

Después se debe correr el servidor
```
./bin/server
```

Finalmente se debe correr los clientes (cuantos quiera)
```
./bin/client 127.0.0.1 8888
```
