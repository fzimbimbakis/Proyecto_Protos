case $2 in
  "netstat")
    netstat -nlp  | grep server;;
  "integrity-velocity")
    time curl -x socks5h://$1@127.0.0.1:1080 http://foo.leak.com.ar| md5sum;;
  "multi-connection")
    while true; do curl -x socks5h://$1@127.0.0.1:1080 http://foo.leak.com.ar; done;;
  "curl")
    curl -x socks5h://$1@127.0.0.1:1080 http://foo.leak.com.ar;;
  "invalid-ipv4-curl")
    curl -x socks5://$1@127.0.0.1:1080 'http://127.0.0.1:3333';;
  "invalid-ipv6-curl")
      curl  -x socks5://$1@127.0.0.1:1080 'http://[::1]:3333';;
  "invalid_dns-curl")
      curl -x socks5h://$1@127.0.0.1:1080 'http://xxxxxxxxxxx/';;
  "dns-ipv6")
      curl -x socks5h://$1@127.0.0.1:1080 http://ipv6.leak.com.ar/;;
  "multi-ip")
     curl -x socks5h://$1@127.0.0.1:1080 http://tpe.proto.leak.com.ar/;;
   "http-proxy")
    http_proxy="" curl http://127.0.0.1:1080/foo.leak.com.ar;;
esac
