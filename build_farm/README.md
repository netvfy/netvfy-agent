# Build the windows CLI agent
docker build -t "netvfy-cli" -f ./Dockerfile-windows-cli .
docker run -it --cidfile=/tmp/netvfy-agent-builder.cid  -v ~/coding/netvfy-agent:/usr/src/netvfy-agent:ro netvfy-cli
docker cp $(cat /tmp/netvfy-agent-builder.cid):/tmp/netvfy-agent-cli_x86.exe /tmp//netvfy-agent-cli_x86.exe

# Build the windows GUI agent
docker build -t "netvfy-gui" -f ./Dockerfile-windows-gui .
docker run -it --cidfile=/tmp/netvfy-agent-builder.cid  -v ~/coding/netvfy-agent:/usr/src/netvfy-agent:ro netvfy-gui
docker cp $(cat /tmp/netvfy-agent-builder.cid):/tmp/netvfy-agent-gui_x86.exe /tmp//netvfy-agent-gui_x86.exe

