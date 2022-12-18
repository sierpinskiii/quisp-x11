FROM ghcr.io/sfc-aqua/quisp-dev:latest

RUN apt-get update && apt-get install -y \
    xserver-xephyr\
    xpra\
    openssh-server\
    libcanberra-gtk-module\
    libcanberra-gtk3-module\
    xauth\
    xdm\
    git \
    doxygen

# Copied command from https://github.com/rogaha/docker-desktop/blob/master/Dockerfile
# Configuring xdm to allow connections from any IP address and ssh to allow X11 Forwarding. 
RUN sed -i 's/DisplayManager.requestPort/!DisplayManager.requestPort/g' /etc/X11/xdm/xdm-config
RUN sed -i '/#any host/c\*' /etc/X11/xdm/Xaccess
RUN ln -s /usr/bin/Xorg 
RUN echo X11Forwarding yes >> /etc/ssh/ssh_config

RUN mkdir /var/run/sshd
RUN echo 'root:coffeeplease' | chpasswd
RUN echo 'vscode:morecoffee' | chpasswd
RUN sed -i 's/.*PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd
ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile
EXPOSE 22
CMD ["/usr/sbin/sshd", "-D"]

# RUN export DISPLAY=localhost:10.0
