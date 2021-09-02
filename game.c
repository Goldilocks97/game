#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <math.h>

#ifndef INBUFSIZE
#define INBUFSIZE 1024
#endif

typedef struct tag_list {
	char *word;
	struct tag_list *next;
} list;

typedef struct tag_sess_var {
	int s_fd;
	char buf[INBUFSIZE];
	int buf_used;
	int num;
	int good;
	int raw;
	int error;
	int bankrupt;
	int buy;
	int acpt_buy;
	int buy_price;
	int sell;
	int sell_price;
	int product;
	int money;
	int factory;
	int ready;
	int build[5];
	struct tag_sess_var *next;
} sess_var;

typedef struct tag_server_var {
	int ls;
	int num_usrs;
	int cur_usrs;
	int turn;
	int sell;
	int sell_price;
	int buy;
	int buy_price;
	int lvl;
	sess_var *head;
} server_var;

int is_number(const char *str)
{
	int i, num = 0;
	
	for (i = 0; str[i] != '\0'; i++) {
		if ((str[i] < '0') || (str[i] > '9'))
			return -1;
		num = (num * 10) + (str[i] - '0');
	}
	return num;
}

void sess_send_msg(const char* msg, int fd)
{
	write(fd, msg, strlen(msg));
}

void server_send_msg(const char *msg, const sess_var *head)
{
	while (head) {
		sess_send_msg(msg, head->s_fd);
		head = head->next;
	}
}

int server_set(server_var *server, int port, int players)
{
	int sock, opt = 1;
	struct sockaddr_in addr;
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket");
		return -1;
	}
	
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
		perror("bind");
		return -1;
	}
	
	if (listen(sock, 5) != 0) {
		perror("listen");
		return -1;
	}
	server->ls = sock;
	server->lvl = 3;
	server->num_usrs = players;
	server->cur_usrs = 0;
	server->turn = 0;
	server->head = NULL;
	return 0;
}

void help(int fd)
{
	char msg[700];

	sprintf(msg, 
			"You can use the following commands:\n"
			"* market - to know about market condition\n"
			"* player N - to know about player condition\n"
			"* prod N - to make N goods\n"
			"* sell N M - to sell N goods at M price each\n"
			"* buy N M - to buy N goods at M price each\n"
			"* build - to build a factory\n"
			"* ready - to finish turn\n"
			"* help - to know information about commands\n");
	sess_send_msg(msg, fd);
}

void add_sess(sess_var *sess, sess_var **head)
{
	sess_var *last;
	
	if (*head) {
		last = *head;
		while (last->next)
			last = last->next;
		last->next = sess;
	} else
		*head = sess;
}

void init_sess(sess_var **ptr, int num, int sock)
{
	int i;
	
	(*ptr)->num = num;
	(*ptr)->s_fd = sock;
	(*ptr)->ready = 0;
	(*ptr)->error = 0;
	(*ptr)->buf_used = 0;
	(*ptr)->raw = 4;
	(*ptr)->product = 0;
	(*ptr)->good = 2;
	(*ptr)->acpt_buy = 0;
	(*ptr)->sell = 0;
	(*ptr)->buy = 0;
	(*ptr)->bankrupt = 0;
	(*ptr)->buy_price = 0;
	(*ptr)->sell_price = 0;
	for (i = 0; i < 5; i++)
		((*ptr)->build)[i] = 0;
	(*ptr)->money = 10000;
	for (i = 0; i < INBUFSIZE; i++) //mb only buf[i] = '\0' ??
		((*ptr)->buf)[i] = '\0';
	(*ptr)->factory = 2;
	(*ptr)->next = NULL;
}

int get_size(int value)
{
	int size;

	for (size = 0; value != 0; size++)
		value = value / 10;
	return size;
}

void server_desc(const server_var *server)
{
	int size1, size2, n1, n2;
	char *msg;

	n1 = server->cur_usrs;
	n2 = server->num_usrs - server->cur_usrs;
	size1 = get_size(n1);
	size2 = get_size(n2);
	msg = malloc(80 + size1 + size2);
	sprintf(msg,
				"Online: %d players.\n"
				"Waiting for: %d players.\n",
				n1, n2);
	server_send_msg(msg, server->head);
	free(msg);
} 

void no_place(int ls)
{
	int res;

	res = accept(ls, NULL, NULL);
	if (res == -1)
		return;
	sess_send_msg("Game has started. Try again late...\n", res);
	shutdown(res, 2);
	close(res);
}

int server_connect(server_var *server)
{
	int res;
	sess_var *tmp;

	if (!server->turn) {
		res = accept(server->ls, NULL, NULL);
		if (res == -1) {
			perror("accept");
			return -1;
		}
		server->cur_usrs++;
		tmp = malloc(sizeof(sess_var));
		init_sess(&tmp, server->cur_usrs, res);
		add_sess(tmp, &server->head);
		server_desc(server);
	} else
		no_place(server->ls);
	return 0;
}

void delete_sess(int sock, sess_var **head)
{
	sess_var *tmp, *ptr;
	
	if ((*head)->s_fd != sock) {
		ptr = *head;
		tmp = ptr->next;
		while (tmp) {
			if (tmp->s_fd == sock) {
				ptr->next = tmp->next;
				free(tmp);
				return;
			}
			ptr = tmp;
			tmp = tmp->next;
		}
	} else {
		tmp = *head;
		*head = (*head)->next;
		if (*head == NULL)
			printf("LOLOLLOLOL'\n");
		free(tmp);
		if (*head == NULL)
			printf("intresting..\n");
	}
}

void session_close(int sock, server_var *server)
{
	char msg[300];

	close(sock);
	server->cur_usrs--;
	sprintf(msg, "Player №%d has disconnected\n", 114311);
	if ((!server->turn) && (server->cur_usrs != 0))
		server_desc(server);
	else {
		sprintf(msg, "Only %d players left\n", server->cur_usrs);	
		server_send_msg(msg, server->head);
	}
	if(server->turn)
		server->num_usrs--;
	delete_sess(sock, &(server->head));
	if (server->head)
		printf("ONYGAD\n");
	if (server->head == NULL)
		printf("ALL I WANTED\n");
}

void announce_result(sess_var *head)
{
	sess_var *ptr;
	int num = -1;
	char msg[200];

	ptr = head;
	while (ptr) {
		if (!(ptr->bankrupt)) {
			num = ptr->num;
			break;
		}
		ptr = ptr->next;
	}
	if (num != -1)
		sprintf(msg, 
					"The winner is player №%d\n"
					"Game over...\n",
					num);
	else
		sprintf(msg, "Tie!\n");
	server_send_msg(msg, head);

}

void sess_close_all(sess_var *head)
{
	sess_var *tmp;
	
	while (head) {
		shutdown(head->s_fd, 1);
		tmp = head;
		head = head->next;
		free(tmp);
	}	
}

void server_close(server_var *server)
{
	
	if (server->num_usrs != 0) {
		announce_result(server->head);
		sess_close_all(server->head);
	}
	shutdown(server->ls, 2);
	close(server->ls);
	exit(1);
}

void copy_str(char *str1, const char *str2, int n)
{
	int i;

	for (i = 0; i < n; i++)
		str1[i] = str2[i];
}

int find_ch(const char *buf, int size, char c)
{
	int i;

	for (i = 0; i < size; i++)
		if (buf[i] == c)
			return i;
	return -1;
}

char *make_massage(const char *str, int value)
{
	char *massage;
	int size;
	
	size = get_size(value);
	massage = malloc(size + 20);
	sprintf(massage, "%s %d.\n", str, value);
	return massage;
}

int cmp_str(const char *str1, const char *str2)
{
	int i, len1, len2;
	
	len1 = strlen(str1);
	len2 = strlen(str2);
	if (len1 != len2)
		return 0;
	for (i = 0; str1[i] != '\0'; i++)
		if (str1[i] != str2[i])
			return 0;
	return 1;
}

int read_word(char **dest, char *source)
{
	char c;
	char *buf;
	int i = 0, size = 10;
	
	buf = malloc(size);
	while (((c = source[i]) != ' ') && (c != '\0')) {
		if ((size - 1) == i) {
			size = size * 2;
			buf = realloc(buf, size);
			if (!buf) {
				perror("realloc");
				exit(1);
			}
		}
		buf[i] = c;
		i++;
	}
	buf[i] = '\0';
	if (c == '\0') {
		*dest = buf;
		return -1;
	}
	if (i == 0) {
		*dest = NULL;
		free(buf);
	} else
		*dest = buf;
	return i+1;
}

list *get_last(list *head)
{
	while (head->next)
		head = head->next;
	return head;
}

void add_word(list **head, char *cmd)
{
	list *tmp, *last;
	
	if ((cmd != NULL) && (cmd[0] != '\0')) {
		if (*head) {
			tmp = malloc(sizeof(list));
			tmp->word = cmd;
			tmp->next = NULL;
			last = get_last(*head);
			last->next = tmp;
		} else {
			*head = malloc(sizeof(list));
			(*head)->word = cmd;
			(*head)->next = NULL;
		}
	}
}

list *get_list(char *line)
{
	int res = 0, cnt = 0;
	list *head = NULL;
	char *word = NULL;
	
	while (1) {
		res = read_word(&word, line+cnt);
		add_word(&head, word);
		cnt += res;
		if (res == -1)
			break;
	}
	return head;
}

int listlen(list *head)
{
	int n = 0;
	
	while (head) {
		head = head->next;
		n++;
	}
	return n;
}

void sess_send_bankr(int fd)
{
	sess_send_msg("You can use only:\n"
					"* help\n"
					"* player N\n", fd);
}

void market(const sess_var *session, const server_var *server)
{
	char msg[280];

	if (session->bankrupt) {
		sess_send_bankr(session->s_fd);
		return;
	}
	sprintf(msg, 
				"Current month is %dth\n"
				"Players still active:\n"
				"*\t\t\t%d\n"
				"Bank sells: items min.price\n"
				"*\t\t\t%d\t%d\n"
				"Bank buys: item max.price\n"
				"*\t\t\t%d\t%d\n", server->turn,
				server->cur_usrs, server->buy,
				server->buy_price, server->sell,
				server->sell_price);
	sess_send_msg(msg, session->s_fd);
}

void error(sess_var *session)
{
	if (session->ready || session->bankrupt) {
		sess_send_bankr(session->s_fd);
		return;
	}
	sess_send_msg("Sorry, you can't do it.\n", session->s_fd);
	session->error++;
	if (session->error == 3)
		sess_send_msg("Note: \"help\" shows "
					"all commands\n", session->s_fd);
}

void list_clean(list *head)
{
	list *tmp;
	
	while (head) {
		tmp = head;
		head = head->next;
		free(tmp->word);
		free(tmp);
	}
}

void player(int num, sess_var *session, server_var *server)
{
	int i, cnt = 0;
	char msg[300];
	sess_var *ptr = NULL;

	if ((num > server->cur_usrs) || (num <= 0)) {
		sprintf(msg, "There are only %d players...\n", server->cur_usrs);
		sess_send_msg(msg, session->s_fd);
		return;
	}
	ptr = server->head;
	while (ptr) {
		if(ptr->num == num)
			break;
		ptr = ptr->next;
	}
	if (!ptr) {
		sess_send_msg("Player left game.\n", session->s_fd);
	}
	for (i = 0; i < 5; i++)
		cnt += (ptr->build)[i];
	if (!ptr->bankrupt)
		sprintf(msg, 
				"Player №%d\n"
				"Money:\n"
				"%d\n"
				"Raw:\n"
				"%d\n"
				"Products:\n"
				"%d\n"
				"Factories:\n"
				"%d\n"
				"Building Factories:\n"
				"%d\n",
				num,
				ptr->money,
				ptr->raw,
				ptr->good,
				ptr->factory,
				cnt);
	else
		sprintf(msg, "Player №%d is a bankrupt\n", ptr->num);
	sess_send_msg(msg, session->s_fd); 
}

void build(sess_var *session)
{
	if (session->ready || session->bankrupt) {
		sess_send_msg("You can use only:\n"
					"* help\n"
					"* player N\n",
					session->s_fd);
		return;
	}
	if (session->money < 5000) {
		sess_send_msg("Not enough money.\n", session->s_fd);
		return;
	}
	sess_send_msg("Application accepted.\n", session->s_fd);
	session->money -= 5000;
	(session->build)[0]++;
}

void ready(sess_var *session)
{
	if (session->bankrupt) {
		sess_send_msg("You can use only:\n"
					"* help\n"
					"* player N\n",
					session->s_fd);
		return;
	}
	if (session->ready == 1) {
		sess_send_msg("Ok. I understand it first time...\n",
						session->s_fd);
		return;
	}
	session->ready = 1;
	sess_send_msg("Your turn is over.\n", session->s_fd);
}

void prod(int num, sess_var *session)
{
	int sum = num * 2000;

	if (session->ready || session->bankrupt) {
		sess_send_msg("You can use only:\n"
					"* help\n"
					"* player N\n",
					session->s_fd);
		return;
	}
	if ((num <= 0) || (num > session->factory)) {
		sess_send_msg("Incorrect data. Try again.\n", session->s_fd);
		return;
	}
	if ((session-> money  < sum) || (session->raw < num)) {
		sess_send_msg("You don't have enough resources.\n",
						session->s_fd);
		return;
	}
	sess_send_msg("Application accepted.\n", session->s_fd);
	session->money -= sum;
	session->product += num;
}

void buy(int num, int price, sess_var *session, server_var *server)
{
	if (session->ready || session->bankrupt) {
		sess_send_msg("You can use only:\n"
					"* help\n"
					"* player N\n",
					session->s_fd);
		return;
	}
	if (session->buy != 0) {
	sess_send_msg("Your application has "
						"been accepted already.\n",
						session->s_fd);
		return;
	}
	if ((num <= 0) ||
		(price <= 0) ||
		(price < server->buy_price) ||
		(session->money < num * price) ||
		(num > server->buy)) {
		sess_send_msg("Incorrect data. Try again.\n", session->s_fd);
		return;
	}
	sess_send_msg("Application accepted.\n", session->s_fd);
	session->buy = num;
	session->buy_price = price;
}

void sell(int num, int price, sess_var *session, server_var *server)
{
	if (session->ready || session->bankrupt) {
		sess_send_msg("You can use only:\n"
					"* help\n"
					"* player N\n",
					session->s_fd);
		return;
	}
	if (session->sell != 0) {
		sess_send_msg("Your application has "
						"been accepted already.\n",
						session->s_fd);
		return;
	}
	if ((num <= 0) ||
		(price <= 0) ||
		(price > server->sell_price) ||
		(num > server->sell)) {
		sess_send_msg("Incorrect data. Try again.\n", session->s_fd);
		return;
	}
	sess_send_msg("Application accepted.\n", session->s_fd);
	session->sell = num;
	session->sell_price = price;
}

void exec_cmd(char *line, sess_var *session, server_var *server)
{
	int len, fd, num, price;
	list *p = NULL;
	
	p = get_list(line);
	len = listlen(p);
	switch (len) {
		case 3:
			if ((cmp_str(p->word, "buy")) &&
				((num = is_number(p->next->word)) != -1) &&
				(((price = is_number(p->next->next->word))) != -1)) {
				buy(num, price, session, server);
				break;;
			}
			if ((cmp_str(p->word, "sell")) &&
				((num = is_number(p->next->word)) != -1) &&
				(((price = is_number(p->next->next->word))) != -1)) {
				sell(num, price, session, server);
				break;
			}
		case 2:
			if ((cmp_str(p->word, "player")) && 
				((fd = is_number(p->next->word)) != -1)) {
					player(fd, session, server);
					break;
			} 
			if ((cmp_str(p->word, "prod")) && 
				(fd = is_number(p->next->word) != -1)) {
				prod(fd, session);
				break;
			}
		case 1:
			if (cmp_str(p->word, "market")) {
				market(session, server);
				break;
			}
			if (cmp_str(p->word, "build")) {
				build(session);
				break;
			}
			if (cmp_str(p->word, "ready")) {
				ready(session);
				break;
			}
			if (cmp_str(p->word, "help")) {
				help(session->s_fd);
				break;
			}
		default:
			error(session);
	}
	list_clean(p);
}

void skip_space(char **str)
{
	int i = 0, pos = 0, flag = 0;

	while ((*str)[pos] == ' ')
		pos++;
	while ((*str)[pos] != '\0') {
		if ((*str)[pos] == ' ') {
			flag = 1;
			break;
		}
		(*str)[i] = (*str)[pos];
		pos++;
		i++;
	}
	if (flag) {
		while ((*str)[pos] != '\0') {
			if ((*str)[pos] != ' ') {
				(*str)[0] = '\0';
				return;
			}
			pos++;
		}
	}
	(*str)[i] = '\0';
}

void is_ready(sess_var *session, server_var *server)
{
	int pos;
	char *line;
	
	if ((pos = find_ch(session->buf, session->buf_used, '\n')) == -1)
		return;
	line = malloc(pos + 1);
	copy_str(line, session->buf, pos);
	line[pos] = '\0';
	memmove(session->buf, session->buf+pos, pos+1);
	session->buf_used -= (pos + 1);
	if (line[pos-1] == '\r')
		line[pos-1] = '\0';
	if (!server->turn) {
		server_send_msg("Game has not started yet...\n", server->head);
		return;
	}
	exec_cmd(line, session, server);
	free(line);
}

int sess_read(sess_var *session, server_var *server)
{
	int res, used = session->buf_used;

	res = read(session->s_fd, session->buf + used, INBUFSIZE - used);
	if (res <= 0)
		return 0;
	session->buf_used += res;
	is_ready(session, server);
	if(session->buf_used >= INBUFSIZE) {
		sess_send_msg("Line too long! Good bye...\n", session->s_fd);
		return 0;
	}
	return 1;
}

void market_set(server_var *server)
{
	float rnum, gnum;
	int rpr, gpr;

	switch (server->turn) {
		case 1:
			rnum = 1.0;
			gnum = 3.0;
			rpr = 800;
			gpr = 6500;
		case 2:
			rnum = 1.5;
			gnum = 2.5;
			rpr = 650;
			gpr = 6000;
		case 3:
			rnum = 2.0;
			gnum = 2.0;
			rpr = 500;
			gpr = 5500;
		case 4:
			rnum = 2.5;
			gnum = 1.5;
			rpr = 400;
			gpr = 5000;
		case 5:
			rnum = 3.0;
			gnum = 1.0;
			rpr = 300;
			gpr = 4500;
	}
	server->buy = (int) floorf(rnum * server->cur_usrs);
	server->buy_price = rpr;
	server->sell = (int) floorf(gnum * server->cur_usrs);
	server->sell_price = gpr;
}

void game_start(server_var *server)
{
	sess_var *ptr;
	
	server->turn = 1;
	market_set(server);
	ptr = server->head;
	server_send_msg("-----GAME STARTED-----\n", server->head);
	while (ptr) {
		help(ptr->s_fd);
		ptr = ptr->next;
	}
	
}

int server_is_newturn(sess_var *head)
{
	int flag = 1;
	sess_var *ptr;
	
	ptr = head;
	while (ptr) {
		if (ptr->ready == 0)
			return 0;
		ptr = ptr->next;
	}
	return flag;
}

void reverse_arr(int *build)
{
	int t = 0, z, i;

	/*t = build[0];
	build[0] = build[4];
	build[4] = t;
	t = build[4];
	build[4] = build[3];
	build[3] = t;
	t = build[1];
	build[1] = build[2];
	build[2] = t;
	t = build[1];
	build[1] = build[3];
	build[3] = t;
	*/
	for (i = 0; i < 5; i++) {
		z = build[i];
		build[i] = t;
		t = z;
	}
}

int sess_endturn(sess_var *session)
{
	int sum = 0, fac = (session->build)[4];

	sum = (300 * session->raw) + 
			(500 * session->good) + 
			(1000 * session->factory);
	session->money -= sum;	
	if (fac != 0) {
		session->money -= fac * 5000;
		session->factory += fac;
		(session->build)[4] = 0;
	}
	reverse_arr(session->build);
	if (session->money < 0) {
		session->bankrupt = 1;
		sess_send_msg("You are a bankrupt.\n"
						"Command \"help\" to know what you can\n",
						session->s_fd);
		return -1;
	}
	session->good += session->product;
	session->raw += session->acpt_buy;
	session->buy = 0;
	session->acpt_buy = 0;
	session->buy_price = 0;
	session->sell = 0;
	session->sell_price = 0;
	session->ready = 0;
	session->product = 0;
	return 0;
}

typedef struct tag_auct_list {
	sess_var *who;
	int num;
	int price;
	struct tag_auct_list *next;
} auct_list;

void auct_buy_inf(int player, int num, int spend, server_var *server)
{
	char msg[200];

	sprintf(msg, 
				"Player №%d buy %d "
				"raw at price %d.\n",
				player, num, spend);
	server_send_msg(msg, server->head);
}

void add_buyer(sess_var *session, auct_list **head)
{
	auct_list *tmp, *ptr;
	int sum;
	
	if (session->buy != 0) {
		sum = session->buy * session->buy_price;
		if (session->money < sum) {
			session->bankrupt = 1;
			return;
		}
		if (*head) {
			tmp = malloc(sizeof(auct_list));
			tmp->num = session->buy;
			tmp->price = session->buy_price;
			tmp->who = session;
			tmp->next = NULL;
			if (tmp->price > (*head)->price) {
				tmp->next = *head;
				*head = tmp;
			} else {
				ptr = *head;
				while ((ptr->next) && (ptr->price > tmp->price))
					ptr = ptr->next;
				if (!ptr->next)
					ptr->next = tmp;
				else {
					tmp->next = ptr->next;
					ptr->next = tmp;
				}
			}
		} else {
			*head = malloc(sizeof(auct_list));
			(*head)->num = session->buy;
			(*head)->price = session->buy_price;
			(*head)->who = session;
			(*head)->next = NULL;
		}
	}
}	

void get_buyers(auct_list **head, server_var *server)
{
	sess_var *ptr;
	
	ptr = server->head;
	while (ptr) {
		if (!(ptr->bankrupt))
			add_buyer(ptr, head);
		ptr = ptr->next;
	}
}

void accept_all_buy(auct_list **head, server_var *server)
{
	auct_list *tmp;
	int max = (*head)->price;
	
	tmp = *head;
	while (tmp->price == max) {
		tmp->who->acpt_buy += tmp->num;
		tmp->who->money -= tmp->num * max;
		auct_buy_inf(tmp->who->num, tmp->num, tmp->num * max, server);
		*head = tmp->next;
		free(tmp);
		tmp = *head;
		if (!*head)
			return;
	}
}

void accept_rand_buy(auct_list **head, server_var *server)
{
	int r, ind, all, cnt = 0,
		max = (*head)->price, sum = server->buy;
	auct_list *tmp;
	float lim;
	
	tmp = *head;
	while (tmp)	{
		if (tmp->price == max)
			cnt++;
		tmp = tmp->next;
	}
	while (cnt > 1) {
		tmp = *head;
		ind = tmp->who->buy;
		if (ind < sum)
			all = ind;
		else
			all = sum;
		lim = (all * 1.0) + 1;
		r = (int)(lim*rand()/RAND_MAX);
		tmp->who->acpt_buy += r;
		sum -= r;
		tmp->who->money -= r * max;
		cnt--;
		if (r != 0)
			auct_buy_inf(tmp->who->num, r, r * max, server);
		*head = tmp->next;
		free(tmp);
	}
	tmp = *head;
	*head = tmp->next;
	tmp->who->acpt_buy = sum;
	tmp->who->money -= sum * max;
	if (sum != 0)
		auct_buy_inf(tmp->who->num, sum, sum * max, server);
	free(tmp);
}

void accept_buy(auct_list **head, server_var *server)
{
	auct_list *tmp;
	int sum = server->buy;

	(*head)->who->acpt_buy = sum;
	(*head)->who->money -= sum * (*head)->price;
	tmp = *head;
	*head = tmp->next;
	auct_buy_inf(tmp->who->num, sum, sum * (*head)->price, server);
	free(tmp);
}

void auct_list_clean(auct_list *head)
{
	auct_list *tmp;
	
	while (head) {
		tmp = head;
		head = head->next;
		free(tmp);
	}
}

void auct_sell_inf(int player, int num, int paid, server_var *server)
{
	char msg[200];

	sprintf(msg, 
				"Player №%d sell %d "
				"goods at price %d.\n",
				player, num, paid);
	server_send_msg(msg, server->head);
}

void auct_buy(server_var *server)
{
	auct_list *head = NULL, *tmp;
	int cnt = 0, sum = 0, max = 0;

	get_buyers(&head, server);
	while (head && server->buy) {
		max = head->price;
		tmp = head;
		while (tmp) {
			if (tmp->price == max) {
				sum += tmp->num;
				cnt++;
			}
			tmp = tmp->next;
		}
		if (sum <= server->buy) {
			accept_all_buy(&head, server);
			server->buy -= sum;
			sum = 0;
			cnt = 0;
		} else { 
			if (cnt == 1)
				accept_buy(&head, server);
			else
				accept_rand_buy(&head, server);
			break;
		}
	}
	auct_list_clean(head);
}

void add_sellers(sess_var *session, auct_list **head)
{
	auct_list *tmp, *ptr;
	
	if (session->sell != 0) {
		if (*head) {
			tmp = malloc(sizeof(auct_list));
			tmp->num = session->sell;
			tmp->price = session->sell_price;
			tmp->who = session;
			tmp->next = NULL;
			if (tmp->price < (*head)->price) {
				tmp->next = *head;
				*head = tmp;
			} else {
				ptr = *head;
				while ((ptr->next) && (ptr->price < tmp->price))
					ptr = ptr->next;
				if (!ptr->next)
					ptr->next = tmp;
				else {
					tmp->next = ptr->next;
					ptr->next = tmp;
				}
			}
		} else {
			(*head) = malloc(sizeof(auct_list));
			(*head)->num = session->sell;
			(*head)->price = session->sell_price;
			(*head)->who = session;
			(*head)->next = NULL;
		}
	}
}

void get_sellers (auct_list **head, server_var *server)
{
	sess_var *ptr;
	
	ptr = server->head;
	while (ptr) {
		if (!(ptr->bankrupt))
			add_sellers(ptr, head);
		ptr = ptr->next;
	}
}

void accept_all_sell(auct_list **head, server_var *server)
{
	auct_list *tmp;
	int min = (*head)->price;
	
	tmp = *head;
	while (tmp->price == min) {
		tmp->who->good -= tmp->num;
		tmp->who->money += tmp->num * min;
		auct_sell_inf(tmp->who->num,
						tmp->num,
						tmp->num*min,
						server);
		*head = tmp->next;
		free(tmp);
		tmp = *head;
		if (!*head)
			return;
	}
}

void accept_sell(auct_list **head, server_var *server)
{
	auct_list *tmp;
	int sum = server->sell;

	(*head)->who->good -= sum;
	(*head)->who->money += sum * (*head)->price;
	tmp = *head;
	auct_sell_inf(tmp->who->num, sum, sum * (*head)->price, server);
	*head = tmp->next;
	free(tmp);
}

void accept_rand_sell(auct_list **head, server_var *server)
{
	int r, cnt = 0, ind, all, 
		min = (*head)->price, sum = server->sell;
	auct_list *tmp;
	float lim;
	
	tmp = *head;
	while (tmp) {
		if (tmp->price == min)
			cnt++;
		tmp = tmp->next;
	}
	while (cnt > 1) {
		tmp = *head;
		ind = tmp->who->sell;
		if (ind < sum)
			all = ind;
		else 
			all = sum;
		lim = (all * 1.0) + 1;
		r = (int)(lim*rand()/RAND_MAX);
		tmp->who->good -= r;
		sum -= r;
		tmp->who->money += r * min;
		cnt--;
		if (r != 0)
			auct_sell_inf(tmp->who->num, r, r * min, server);
		*head = tmp->next;
		free(tmp);
	}
	tmp = *head;
	*head = tmp->next;
	tmp->who->good -= sum;
	tmp->who->money += sum * min;
	if (sum != 0)
		auct_sell_inf(tmp->who->num, sum, sum * min, server);
	free(tmp);
}

void auct_sell(server_var *server)
{
	auct_list *head = NULL, *tmp;
	int cnt = 0, sum = 0, min = 0;

	get_sellers(&head, server);
	while (head && server->sell) {
		min = head->price;
		tmp = head;
		while (tmp) {
			if (tmp->price == min) {
				sum += tmp->num;
				cnt++;
			}
			tmp = tmp->next;
		}
		if (sum <= server->sell) {
			accept_all_sell(&head, server);
			server->sell -= sum;
			sum = 0;
			cnt = 0;
		} else { 
			if (cnt == 1)
				accept_sell(&head, server);
			else 
				accept_rand_sell(&head, server);
			break;
		}
	}
	auct_list_clean(head);
}

void server_set_lvl(server_var *server)
{
	const int level_change[5][5] = {
		{4, 4, 2, 1, 1},
		{3, 4, 3, 1, 1},
		{1, 3, 4, 3, 1},
		{1, 1, 3, 4, 3},
		{1, 1, 2, 4, 4}
	};
	int i, r, sum = 0, lvl = server->lvl;
	
	r = 1 + (int)(12.0*rand()/(RAND_MAX+1.0));
	for (i = 0; sum < r; i++)
		sum += level_change[lvl-1][i];
	server->lvl = i;
}

void server_endturn(server_var *server)
{
	int res;
	char msg [300];
	sess_var *ptr;
	
	auct_buy(server);
	auct_sell(server);
	ptr = server->head;
	while (ptr) {
		if (!(ptr->bankrupt)) {
			res = sess_endturn(ptr);
			if (res == -1) {
				server->cur_usrs--;
				sprintf(msg,
						"Player №%d is a bankrupt.\n",
						ptr->num);
				server_send_msg(msg, server->head);
			}
		}
		ptr = ptr->next;
	}
	if (server->cur_usrs <= 1)
		server_close(server);
	server->turn++;
	server_send_msg("Next turn.\n", server->head);
	server_set_lvl(server);
	market_set(server);
}

void server_check_state(server_var *server)
{
	if (((server->num_usrs <= 1) ||
		(server->cur_usrs == 1)) &&
		(server->turn)){
		server_close(server);
	}
	if ((server_is_newturn(server->head)) && (server->turn)) {
		server_endturn(server);
	}
	if ((server->turn == 0) && (server->cur_usrs == server->num_usrs))
		game_start(server);
}

int server_start(server_var *server)
{
	fd_set readfds;
	int i = 0, fd, max_d;
	sess_var *ptr;
	
	for (;;) {
		ptr = server->head;
		max_d = server->ls;
		FD_ZERO(&readfds);
		FD_SET(server->ls, &readfds);
		while (ptr) {
			fd = ptr->s_fd;
			FD_SET(fd, &readfds);
			if (fd > max_d)
				max_d = fd;
			ptr = ptr->next;
		}
		if (select(max_d+1, &readfds, NULL, NULL, NULL) == -1) {
			perror("select");
			return 5;
		}
		ptr = server->head;
		if (!ptr)
			printf("it works\n");
		while (ptr) {
			fd = (ptr->s_fd);  //delete fd and use ptr->s_fd
			if (FD_ISSET(fd, &readfds))
				if (sess_read(ptr, server) == 0) {
					printf("Let us go\n");
					session_close(fd, server);
					printf("I guess...\n");
				}
			printf("i is %d\n", i);
			i++;
			if (ptr == NULL)
				break;
			if (ptr != NULL)
				printf("fkomefmer\n");
			printf("1\n");
			ptr = ptr->next;
			printf("2\n");
		}
		if (FD_ISSET(server->ls, &readfds))
			if (server_connect(server) == -1)
				return 6;
		server_check_state(server);
	}
}

int main(int argc, const char * const *argv)
{
	int port, players;
	server_var server;

	if (argc != 3) {
		fprintf(stderr, "Note: server <number_of_players> <port>.\n");
		return 1;
	}
	if (((players = is_number(argv[1])) == -1) ||
		(players > 10) ||
		(players <= 0)) {
		fprintf(stderr, "Invalid number of players.\n");
		return 2;
	}
	if (((port = is_number(argv[2])) == -1) ||
		(port > 30000) ||
		(port <= 0)) {
		fprintf(stderr, "Invalid port number.\n");
		return 3;
	}
	if ((server_set(&server, port, players) != 0))
		return 4;
	
	return server_start(&server);
}
