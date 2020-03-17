protoc -I . --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` client_messages.proto raft_messages.proto 
protoc -I . --cpp_out=. client_messages.proto raft_messages.proto 
