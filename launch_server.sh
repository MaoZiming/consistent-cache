#!/bin/bash

# Build the Docker image
docker build -t consistent-cache .
if [ $? -ne 0 ]; then
    echo "Error: Docker build failed. Exiting."
    exit 1
fi

docker tag consistent-cache registry.dev.databricks.com/universe/consistent-cache
docker push registry.dev.databricks.com/universe/consistent-cache

kubectl delete deployment consistent-cache-server-cpp --namespace=softstore-storelet
kubectl apply -f consistent-cache-server.yaml
sleep 3
kubectl get pods -n softstore-storelet -l app=consistent-cache-server-cpp -o wide

kubectl get pods -n softstore-storelet -l app=consistent-cache-server-cpp -o name | sed 's|pod/||' | xargs -I {} kubectl top pod {} -n softstore-storelet --no-headers | awk '{sum+=$2} END {print sum "m"}'