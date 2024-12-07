#!/bin/bash

# Build the Docker image
docker build -t consistent-cache .
if [ $? -ne 0 ]; then
    echo "Error: Docker build failed. Exiting."
    exit 1
fi

docker tag consistent-cache registry.dev.databricks.com/universe/consistent-cache
docker push registry.dev.databricks.com/universe/consistent-cache

kubectl delete deployment consistent-cache-client-cpp --namespace=softclient
kubectl apply -f consistent-cache-client.yaml


kubectl get pods -n softclient -l app=consistent-cache-client-cpp -o name | sed 's|pod/||' | xargs -I {} kubectl top pod {} -n softclient --no-headers | awk '{sum+=$2} END {print sum "m"}'