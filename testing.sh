case $3 in
  "netstat")
    netstat -nlp  | grep server;;
  "integrity-velocity")
    time curl -x socks5h://$1@127.0.0.1:$2 http://foo.leak.com.ar| md5sum;;
  "multi-connection")
    while true; do curl -x socks5h://$1@127.0.0.1:$2 http://foo.leak.com.ar; done;;
  "curl")
    curl -x socks5h://$1@127.0.0.1:$2 http://foo.leak.com.ar;;
  "invalid-ipv4-curl")
    curl -x socks5://$1@127.0.0.1:$2 'http://127.0.0.1:3333';;
  "invalid-ipv6-curl")
      curl  -x socks5://$1@127.0.0.1:$2 'http://[::1]:3333';;
  "invalid_dns-curl")
      curl -x socks5h://$1@127.0.0.1:$2 'http://xxxxxxxxxxx/';;
  "dns-ipv6")
      curl -x socks5h://$1@127.0.0.1:$2 http://ipv6.leak.com.ar/;;
  "multi-ip")
     curl -x socks5h://$1@127.0.0.1:$2 http://tpe.proto.leak.com.ar/;;
   "http-proxy")
    http_proxy="" curl http://127.0.0.1:$2/foo.leak.com.ar;;
esac
