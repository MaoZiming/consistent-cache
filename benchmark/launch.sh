docker build -t hello-world-cpp .
docker tag hello-world-cpp registry.dev.databricks.com/universe/hello-world-cpp
docker push registry.dev.databricks.com/universe/hello-world-cpp

cd ~/universe
kubectl delete pod hello-world-cpp -n softclient
kubectl apply -f cpp/third_party/consistent_cache/benchmark/hello-world-cpp-pod.yaml