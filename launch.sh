docker build -t consistent-cache .
docker tag consistent-cache registry.dev.databricks.com/universe/consistent-cache
docker push registry.dev.databricks.com/universe/consistent-cache

kubectl delete pod consistent-cache-server-cpp -n softstore-storelet
kubectl apply -f consistent-cache-server.yaml

kubectl delete pod consistent-cache-client-cpp -n softclient
kubectl apply -f consistent-cache-client.yaml