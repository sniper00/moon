# Moon Cluster Communication Example

1. Start the cluster ETC service to get node configuration information
   ```shell
   ../../moon cluster_etc.lua node.json
   ```
2. Start the receiver node
   ```shell
   ../../moon node.lua 2
   ```
3. Start the sender node
   ```shell
   ../../moon node.lua 1
   ```
