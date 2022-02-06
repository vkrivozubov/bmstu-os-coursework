#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <linux/netlink.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#define UNIQUE_MODULE 20
#define USER_PORT 100
#define MAX_PLOAD 125
#define MSG_LEN 125

typedef struct _user_msg_info
{
    struct nlmsghdr hdr;
    char  msg[MSG_LEN];
} user_msg_info;

int main(int argc, char **argv)
{
    int skfd;
    int ret;
    user_msg_info u_info;
    socklen_t len;
    struct nlmsghdr *nlh = NULL;
    struct sockaddr_nl saddr, daddr;
    char *umsg = "hello netlink!!";
    char *connection_notification = "RECEIVER CONNECTED";
    char *ready_to_new_package = "CLIENT READY FOR NEW PACKAGE";
    int loop_count = 0;

   /*Create netlink socket*/
    skfd = socket(AF_NETLINK, SOCK_RAW, UNIQUE_MODULE);//Create a socket using user defined protocol NETLINK_TEST.
    if(skfd == -1)
    {
        perror("create socket error uuuuuu\n");
        return -1;
    }

   //Source address.
    memset(&saddr, 0, sizeof(saddr));
    saddr.nl_family = AF_NETLINK;//AF_NETLINK
    saddr.nl_pid = USER_PORT; //netlink portid, same as kernel.
    saddr.nl_groups = 0;
    if(bind(skfd, (struct sockaddr *)&saddr, sizeof(saddr)) != 0)//bind to skfd with saddr.
    {
        perror("bind() error\n");
        close(skfd);
        return -1;
    }

   //Destination address.
    memset(&daddr, 0, sizeof(daddr));
    daddr.nl_family = AF_NETLINK;
    daddr.nl_pid = 0;   //to kernel 
    daddr.nl_groups = 0;

    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PLOAD));
    memset(nlh, 0, sizeof(struct nlmsghdr));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PLOAD);
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_type = 0;
    nlh->nlmsg_seq = 0;
    nlh->nlmsg_pid = saddr.nl_pid;//self port

    memcpy(NLMSG_DATA(nlh), connection_notification, strlen(connection_notification));

    printf("sendto kernel:%s\n", umsg);
    ret = sendto(skfd, nlh, nlh->nlmsg_len, 0, (struct sockaddr *)&daddr, sizeof(struct sockaddr_nl));
    if(!ret)
    {
        perror("sendto error\n");
        close(skfd);
        exit(-1);
    }
    while(loop_count < 11) {
       //Receive netlink message from kernel.
        memset(&u_info, 0, sizeof(u_info));
        len = sizeof(struct sockaddr_nl);
        ret = recvfrom(skfd, &u_info, sizeof(user_msg_info), 0, (struct sockaddr *)&daddr, &len);
        if(!ret)
        {
            perror("recv form kernel error\n");
            close(skfd);
            exit(-1);
        }

        memcpy(NLMSG_DATA(nlh), ready_to_new_package, strlen(ready_to_new_package));
        printf("send ready state to kernel:%s\n", umsg);
        ret = sendto(skfd, nlh, nlh->nlmsg_len, 0, (struct sockaddr *)&daddr, sizeof(struct sockaddr_nl));
        if(!ret)
        {
            perror("sendto error\n");
            close(skfd);
            exit(-1);
        }

        printf("from kernel:%s\n", u_info.msg);
   //usleep(1000);
    loop_count++;
    }
    close(skfd);

    free((void *)nlh);
    return 0;
}