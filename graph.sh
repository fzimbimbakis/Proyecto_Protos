socksPort=1080
mngPort=8080
socksCredentials="user:pass"
link=https://old-releases.ubuntu.com/releases/16.10/ubuntu-16.10-desktop-amd64.iso
socksIp="127.0.0.1"
for i in {1..3} ; do
time curl -x socks5h://"$socksCredentials"@"$socksIp":"$socksPort" $link | md5sum &
PID"$i"=$!
done

while true > /dev/null;
do
  sleep 1
  echo 2 | ./bin/client -u admin:admin -P 3001 -f ./statistics/index2 > /dev/null
  echo 3 | ./bin/client -u admin:admin -P 3001 -f ./statistics/index3 > /dev/null
  echo 4 | ./bin/client -u admin:admin -P 3001 -f ./statistics/index4 > /dev/null
  echo 5 | ./bin/client -u admin:admin -P 3001 -f ./statistics/index5 > /dev/null
  echo 6 | ./bin/client -u admin:admin -P 3001 -f ./statistics/index6 > /dev/null
  echo 7 | ./bin/client -u admin:admin -P 3001 -f ./statistics/index7 > /dev/null
  echo 8 | ./bin/client -u admin:admin -P 3001 -f ./statistics/index8 > /dev/null
  echo 9 | ./bin/client -u admin:admin -P 3001 -f ./statistics/index9 > /dev/null
done;