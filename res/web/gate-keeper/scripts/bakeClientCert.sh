#!/bin/bash

SHORT=n:,p:,h
LONG=name:,password:,help
OPTS=$(getopt -a -n bajeClietCert --options $SHORT --longoptions $LONG -- "$@")

eval set -- "$OPTS"

while :
do
  case "$1" in
    -n | --name )
      name="$2"
      shift 2
      ;;
    -p | --password )
      password="$2"
      shift 2
      ;;
    -h | --help)
      "You are on your own"
      exit 2
      ;;
    --)
      shift;
      break
      ;;
    *)
      echo "Unexpected option: $1"
      ;;
  esac
done

clientkeypwd=$(openssl rand -base64 32)
cakeypwd=$(openssl rand -base64 32)
serverkeypwd=$(openssl rand -base64 32)

rm -fr /tmp/clientcert

mkdir /tmp/clientcert

openssl genrsa -aes256 -passout pass:${cakeypwd} -out /tmp/clientcert/ca.pass.key 4096 && \
openssl rsa -passin pass:${cakeypwd} -in /tmp/clientcert/ca.pass.key -out /tmp/clientcert/ca.key && \
openssl req -new -x509 -days 365 -key /tmp/clientcert/ca.key -out /tmp/clientcert/ca.crt \
                    -subj "/C=UK/ST=Warwickshire/L=Leamington/O=OrgName/OU=IT Department/CN=example.com" && \
openssl genrsa -aes256 -passout pass:${serverkeypwd} -out /tmp/clientcert/server.pass.key 4096 && \
openssl rsa -passin pass:${serverkeypwd} -in /tmp/clientcert/server.pass.key -out /tmp/clientcert/server.key && \
openssl req -new -key /tmp/clientcert/server.key -out /tmp/clientcert/server.csr \
            -subj "/C=UK/ST=Warwickshire/L=Leamington/O=OrgName/OU=IT Department/CN=example.com" && \
openssl req -new -x509 -days 365 -in /tmp/clientcert/server.csr -key /tmp/clientcert/ca.key -out /tmp/clientcert/server.crt \
            -subj "/C=UK/ST=Warwickshire/L=Leamington/O=OrgName/OU=IT Department/CN=example.com"

openssl genrsa -aes256 -passout pass:${clientkeypwd} -out /tmp/clientcert/client.pass.key 4096 

openssl rsa -passin pass:${clientkeypwd} -in /tmp/clientcert/client.pass.key -out /tmp/clientcert/client.key

openssl req -new -key /tmp/clientcert/client.key -out /tmp/clientcert/client.csr \
            -subj "/C=UK/ST=Warwickshire/L=Leamington/O=OrgName/OU=${name}/CN=${name}.com"

openssl x509 -req -passin pass:${cakeypwd} -days 365 -in /tmp/clientcert/client.csr -CAcreateserial -CA /tmp/clientcert/ca.crt -CAkey /tmp/clientcert/ca.pass.key -out /tmp/clientcert/client.crt


openssl req -new -x509 -days 365 -passout pass:${clientkeypwd} -in /tmp/clientcert/server.csr -key /tmp/clientcert/ca.key -out /tmp/clientcert/server.crt \
                -subj "/C=UK/ST=Warwickshire/L=Leamington/O=OrgName/OU=${name}/CN=${name}.com"

cat /tmp/clientcert/client.key /tmp/clientcert/client.crt /tmp/clientcert/ca.crt > /tmp/clientcert/client.pem

openssl pkcs12 -export -passout pass:${password} -out /tmp/clientcert/client.pfx -inkey /tmp/clientcert/client.key -in /tmp/clientcert/client.pem -certfile /tmp/clientcert/ca.crt


