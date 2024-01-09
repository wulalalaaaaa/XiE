//xiling_player

#include"xiling_wupin.h"
#include"xiling_jineng.h"
#include "basetools.h"
#include"_log.h"
#include<vector>
#include<ctime>
#include<iostream>


static const std::string youxi_show("目前的小游戏有：\n1、猜炸弹！\n2、老虎机！\n之后每句话都要以“游戏”为开头进入分区哦");

static char wei[2] = { 0,0 };
static short zhadan = -1; static short f_caishu = -1;//炸弹数和上一次的猜数
static char laohuji[5] = { 0, -1,10,0,0 };
//laohuji[0]是加注数，laohuji[1]是游戏人id,  [2]是yaofa，  [3]是yaonage   [4]是已经摇开了哪个
static int beilv = 0;


 laohuji_yuansu yuansu[9] = { gundong ,gundong, gundong ,gundong, gundong ,gundong ,gundong ,gundong, gundong };



 xiling_player::xiling_player(short IDMAX) {
 id = IDMAX++;
 qq = 0;
 name = tostring(id) + "快说_\"属性改名XXX\"，只要10点法力值哦";
 health = 100;
 healthmax = 100;
 magic = 120;
 magicmax = 100;
 jinbi = 100;
 liliang = 9;
 zhili = 12;
 wufang = 5;
 fafang = 5;
}

 std::string xiling_player::show() {
	 std::string a;
	 a = this->name + "\n生命值："+ tostring(this->health)+ "\n法力值："+ tostring(this->magic)+
		 "\n体力："+ tostring<int>(this->tili())+ " 精神："+tostring<int>(this->jingshen())+
		 "\n力量："+tostring<int>(this->liliang)+" 智力："+tostring<int>(this->zhili)+
		 "\n金币数："+ tostring(this->jinbi)+ "\n状态：";
	
	 for (auto value : this->buff) {
		 switch (value) {
		 case die:a += "死亡，";
			 break;
		 case alive:a += "强调性存活中，";
			 break;
		 case hunshui:a += "昏睡，";
			 break;
		 case daliwan:a += "力大无穷，";
			 break;
		 case xuruo:a += "虚弱，";
			 break;
		 case meihuo_acceptance:a += "魅惑";
			 break;
		 case meihuo_attack:
			 break;
		 case yinluan:a += "yin乱，";
			 break;
		 case xuanyun:a += "眩晕，";
			 break;
		 case zhongdu:a += "中毒，";
			 break;
		 case bulaobusi:a += "不老不死，";
			 break;
		 default:a += "[未知状态]";
			 break;
		 }
	 }

	 return a;
 }

 std::string xiling_player::show_beibao(unsigned long long& showqq)
 {
	 if (showqq == qq || qq == 2416314577||showqq==0) {
		 if (beibao.size() == 0) {
			 return "背包里面没东西呢";
		 }
		 std::string shuchu = "";
		 for (int i = 0; i < beibao.size(); i++){
			 shuchu += beibao[i]->m_name + " ";
		 }
		 return shuchu;
	 }
	 else {
		 return "您无权查看他人背包";
	 }
 }

 std::string xiling_player::show_jineng(unsigned long long& showqq)
 {
	 if (showqq == qq || qq == 2416314577) {
		 if (jinengbiao.size() == 0) {
			 return "还没有技能呢（奇怪，？）";
		 }
		 std::string shuchu = "";
		 for (int i = 0; i < jinengbiao.size(); i++) {
			 shuchu += jinengbiao[i]->m_name;
		 }
		 return shuchu;
	 }
	 return "您无权查看他人背包";

 }

 //xiling_dangan
 
void xiling_dangan::addPlayer(std::vector<xiling_player>& _7fang,xiling_player sb, unsigned long long& _qq) {
	 _7fang.push_back(sb);
	 _7fang[sb.id].setqq(_qq);
 }


 //xiling_youxi
 std::string xiling_youxi::caizhadan(int caishu) {
	 if (caishu > 100 || caishu < 0) {
		 return "请输入正确范围的数。。。(为难.jpg)";
	 }
	 if (caishu < zhadan) {
		 if ((f_caishu<caishu&&f_caishu<zhadan)||f_caishu>zhadan) {
			 if (zhadan - caishu > 40) {
				 f_caishu = caishu;
				 return "炸弹比这个数要大哦";
			 }
			 else if (zhadan - caishu > 25) {
				 f_caishu = caishu;
				 return "炸弹比这个大一些~";
			 }
			 else if (zhadan - caishu > 15) {
				 f_caishu = caishu;
				 return "炸弹比这个就大一点点！";
			 }
			 else if (zhadan - caishu > 6) {
				 f_caishu = caishu;
				 return "炸弹比这个只大那么一点！（比划）";
			 }
			 else if (caishu = zhadan) {
				 youxi_clear();
				 return "boommmmmmm——！";
			 }
			 else if (zhadan - caishu < 6) {
				 f_caishu = caishu;
				 return "哇炸弹就在旁边了！(只大一点点点——)";
			 }
			 else { return "[error] 这鬼地方不可能出错"; }
		 }
		 else return "请输入正确范围的数。。。(为难.jpg)";
	 }
	 else if (caishu > zhadan) {
		 if ((f_caishu>zhadan&&f_caishu>caishu)||f_caishu<zhadan) {
			 if (caishu - zhadan > 40) {
				 f_caishu = caishu;
				 return "炸弹比这个数要小哦";
			 }
			 else if (caishu - zhadan > 30) {
				 f_caishu = caishu;
				 return "炸弹比这个小一些";
			 }
			 else if (caishu - zhadan > 15) {
				 f_caishu = caishu;
				 return "炸弹比这个就小一点点！";
			 }
			 else if (caishu - zhadan > 6) {
				 f_caishu = caishu;
				 return "炸弹比这个只小那么一点！（比划）";
			 }
			 else if (caishu - zhadan < 6) {
				 f_caishu = caishu;
				 return "哇炸弹就在旁边了！(只小一点点点点——)";
			 }
			 else { return "[error] 这鬼地方不可能出错"; }

		 }
		 else return "请输入正确范围的数。。。(为难.jpg)";
	 }
	 else if (caishu = zhadan) {
		 youxi_clear();
		 return "boommmmmmm——！";
	 }
	 else return "[error] 这鬼地方不可能出错";
 }

 std::string xiling_youxi::zengmeyao(const char yaofa) {

	 switch (yaofa) {
	 case 0://轻轻地
		 return "轻轻地摇摇杆，听到一道清晰的\"咔咔\"声 \n";

	 case 1://温柔地
		 return "温柔地摇摇杆，摇杆听话地摇动，听到一声明确的\"咔\" \n";

	 case 2://颤抖地
		 return "颤抖地摇摇杆，听到多道机械撞击的声音，你不禁多摇几下 \n";

	 case 3://剧烈地
		 return "剧烈地摇摇杆，摇杆和机器内部发出唧哩桄榔的声音 \n";

	 case 4://巨大力地
		 return "巨大力地摇摇杆，机器发出濒临散架的声音 \n";

	 case 5://癫狂地
		 return "癫狂地摇摇杆。。。别摇了！机器要坏掉了！ \n";

	 case 6://欢乐地
		 return "欢乐地摇摇杆，发出叮铃咔拉的声音 \n";

	 case 7://随便地
		 return "随便地摇摇杆，摇杆随便发了两声 \n";

	 default:
		 //无摇法
		 return "";
	 }

 }

 int xiling_youxi::laohuji_beilv(const int jiazhushu) {
	 int beilv = 0;
	 switch (jiazhushu) {
	 case 3:
		 if (yuansu[0] == yuansu[4] && yuansu[4] == yuansu[8]) {
			 beilv += 1;
			 beilv *= 2;
			 if (yuansu[0] != 64 && yuansu[4] != 64 && yuansu[8] != 64) {
				 beilv *= 2;
				 if (yuansu[0] == qiqi) {
					 beilv *= 7;
				 }
			 }
			 else if ((yuansu[0] != 64 && yuansu[0] == qiqi) || (yuansu[4] != 64 && yuansu[4] == qiqi) || (yuansu[8] != 64 && yuansu[8] == qiqi)) {
				 beilv *= 7;
			 }
		 }
		 if (yuansu[2] == yuansu[4] && yuansu[4] == yuansu[6]) {
			 beilv += 1;
			 beilv *= 2;
			 if (yuansu[2] != 64 && yuansu[4] != 64 && yuansu[6] != 64) {
				 beilv *= 2;
				 if (yuansu[2] == qiqi) {
					 beilv *= 7;
				 }
			 }
			 else if ((yuansu[2] != 64 && yuansu[2] == qiqi) || (yuansu[4] != 64 && yuansu[4] == qiqi) || (yuansu[6] != 64 && yuansu[6] == qiqi)) {
				 beilv *= 7;
			 }
		 }

	 case 2:
		 if (yuansu[0] == yuansu[1] && yuansu[1] == yuansu[2]) {
			 beilv += 1;
			 beilv *= 2;
			 if (yuansu[0] != 64 && yuansu[1] != 64 && yuansu[2] != 64) {
				 beilv *= 2;
				 if (yuansu[1] == qiqi) {
					 beilv *= 7;
				 }
			 }
			 else if ((yuansu[0] != 64 && yuansu[0] == qiqi) || (yuansu[1] != 64 && yuansu[1] == qiqi) || (yuansu[2] != 64 && yuansu[2] == qiqi)) {
				 beilv *= 7;
			 }
		 }
		 if (yuansu[6] == yuansu[7] && yuansu[7] == yuansu[8]) {
			 beilv += 1;
			 beilv *= 2;
			 if (yuansu[6] != 64 && yuansu[7] != 64 && yuansu[8] != 64) {
				 beilv *= 2;
				 if (yuansu[6] == qiqi) {
					 beilv *= 7;
				 }
			 }
			 else if ((yuansu[6] != 64 && yuansu[6] == qiqi) || (yuansu[7] != 64 && yuansu[7] == qiqi) || (yuansu[8] != 64 && yuansu[8] == qiqi)) {
				 beilv *= 7;
			 }
		 }

	 case 1:
		 if (yuansu[3] == yuansu[4] && yuansu[4] == yuansu[5]) {
			 beilv += 1;
			 beilv *= 2;
			 if (yuansu[3] != 64 && yuansu[4] != 64 && yuansu[5] != 64) {
				 beilv *= 2;
				 if (yuansu[3] == qiqi) {
					 beilv *= 7;
				 }
			 }
			 else if ((yuansu[3] != 64 && yuansu[3] == qiqi) || (yuansu[4] != 64 && yuansu[4] == qiqi) || (yuansu[5] != 64 && yuansu[5] == qiqi)) {
				 beilv *= 7;
			 }
		 }
		 return beilv;
	 default:
		 return -1;
	 }



	 return 0;
 }

 std::string xiling_youxi::yao_laohuji(std::vector<xiling_player>& _7fang,const int yaonage, const char yaofa, const char jiazhushu, const short player_id) {

	 std::string jieguo = "";
	 jieguo += zengmeyao(yaofa);

	 //static std::default_random_engine e;
	 //static std::uniform_int_distribution<unsigned> u(1, 7);
	 srand((unsigned)time(NULL));
	 //值拿到了
	 switch (yaonage) {
	 case(0)://全都摇开
		 for (int i = 0; i < 9; i++) {
			 yuansu[i] = yao_suiji(rand() % 7 + 1);
		 }
		 break;
	 case(1)://摇开第一个
		 for (int i = 0; i < 9; ) {
			 yuansu[i] = yao_suiji(rand() % 7 + 1);
			 i += 3;
		 }
		 break;
	 case(2)://摇开第二个
		 for (int i = 1; i < 9; ) {
			 yuansu[i] = yao_suiji(rand() % 7 + 1);
			 i += 3;
		 }
		 break;
	 case(3)://摇开第三个
		 for (int i = 2; i < 9; ) {
			 yuansu[i] = yao_suiji(rand() % 7 + 1);
			 i += 3;
		 }
		 break;
	 case(4)://摇开1、2
		 for (int i = 0; i < 9; ) {
			 yuansu[i] = yao_suiji(rand() % 7 + 1);
			 i += 3;
		 }
		 for (int i = 1; i < 9; ) {
			 yuansu[i] = yao_suiji(rand() % 7 + 1);
			 i += 3;
		 }
		 break;
	 case(5)://摇开2、3
		 for (int i = 1; i < 9; ) {
			 yuansu[i] = yao_suiji(rand() % 7 + 1);
			 i += 3;
		 }
		 for (int i = 2; i < 9; ) {
			 yuansu[i] = yao_suiji(rand() % 7 + 1);
			 i += 3;
		 }
		 break;
	 case(6)://摇开1、3
		 for (int i = 0; i < 9; ) {
			 yuansu[i] = yao_suiji(rand() % 7 + 1);
			 i += 3;
		 }
		 for (int i = 2; i < 9; ) {
			 yuansu[i] = yao_suiji(rand() % 7 + 1);
			 i += 3;
		 }
		 break;
	 default:
		 youxi_clear();
		 std::cout << "[???] function of yao_laohuji,in switch(yaonage),the yaonage is out of size\n";
		 return "error";
	 }

	 jieguo += show_laohuji(yuansu);

	 //如果结束 算倍率 把金币结算了并clear
	 if (laohuji_is_over()) {
		 //1苹果，2芒果，3铃铛，4西瓜，5星星，6七七，7百变
		 std::cout << "摇完啦";
		 beilv = laohuji_beilv((int)laohuji[0]);
		 if (beilv == -1) {
			 std::cout << "[error] something error in place laohuji\n";
			 return "出了奇怪的错误，快通知我！因为有内存泄漏的可能，尽情的信息轰炸我吧（";
		 }
		 _7fang[player_id].jinbi += beilv * 6;
		 if (beilv != 0) {
			 jieguo = jieguo + "滴铃滴铃滴铃滴铃，掉出来" + tostring(beilv * 6) + "枚金币！";

		 }
		 else {
			 jieguo += "当啷当啷当啷，请投币.jpg";

		 }
	 }


	 youxi_clear();
	 return jieguo;
 }

 laohuji_yuansu xiling_youxi::yao_suiji(int u) {

	 switch (u) {
	 case 1:return pingguo;
	 case 2:return mangguo;
	 case 3:return lindang;
	 case 4:return xigua;
	 case 5:return xingxing;
	 case 6:return qiqi;
	 case 7:return baibian;
	 default:
		 youxi_clear();
		 std::cout << "\n【error】the function of yao_suiji()  is error\n" << std::endl;
		 return gundong;
	 }

 }

 std::string xiling_youxi::show_laohuji(laohuji_yuansu yuansu[]) {
	 std::string a = "滴拎滴拎滴拎滴拎\n";
	 for (int j = 0; j < 3; j++) {
		 for (int i = 1 + 3 * j; i < 4 + 3 * j; i++) {
			 a += to_screen(yuansu[i - 1]);
		 }
		 a += "\n";
	 }
	 return a;
 }

 std::string xiling_youxi::to_screen(const laohuji_yuansu laohuji) {
	 switch (laohuji) {
	 case gundong:return "·滚动·"; break;
	 case pingguo:return "·苹果·"; break;
	 case mangguo:return "·芒果·"; break;
	 case lindang:return "·铃铛·"; break;
	 case xigua:return "·西瓜·"; break;
	 case xingxing:return "·星星·"; break;
	 case qiqi:return "·七七·"; break;
	 case baibian:return "·百变·"; break;
	 default:
		 youxi_clear();
		 std::cout << "[error] the function of to_screen,laohuji is out of size" << std::endl;
		 return "error";
	 }

 }

 bool xiling_youxi::laohuji_is_over() {
	 for (int i = 0; i < 9; i++) {
		 if (yuansu[i] == gundong) {
			 return 0;
		 }
	 }
	 return 1;
 }

 std::string xiling_youxi::send(std::vector<xiling_player>& _7fang,std::string& hear, short player_id) {
	 int xun = 1;
	 while (xun--) {
		 switch (wei[0]) {

		 case(0)://无进度，处理选择，show（）
			 if (getint<int>(hear) > 0 && getint<int>(hear) < 3) {
				 wei[0] += getint<int>(hear);
				 xun++;
				 continue;
			 }
			 else return youxi_show;

		 case(1)://猜炸弹！
			 switch (wei[1]) {
			 case(0)://新游戏，无进度
				 srand((unsigned)time(NULL));
				 zhadan = rand() % 100;
				 wei[1]++;
				 return "炸弹埋好啦，请在1~100之内猜哦\n像这样:\"游戏50\"";
			 case(1):
				 return caizhadan(getint<int>(hear));
			 default:
				 std::cout << "[error] progress of caizhadan is over size\n";
				 return "出故障了，可通知我去修";
			 }

		 case(2)://老虎机！苹果，芒果，铃铛，西瓜，星星，七七，百变
			 switch (wei[1]) {
			 case(0)://新游戏，无进度
				 if (hear.find("投币") != std::string::npos || hear.find("加注") != std::string::npos) {
					 wei[1]++;
					 laohuji[0]++;//加注数
					 laohuji[1] = player_id;//游戏人
					 _7fang[player_id].jinbi -= 2;

					 return "投币成功！";
				 }
				 else { return "请投币（1注2币）投币后摇摇杆就好了\n像这样：\"游戏我要癫狂地摇摇杆#！&*#！\""; }
			 case(1):
			 case(2):
				 if (hear.find("投币") != std::string::npos || hear.find("加注") != std::string::npos) {
					 if (laohuji[0] == 3) {
						 return "最高注为三注，三注：三倍率，同时三横行两斜杠之任一行相同即中，多行中多重倍率，同时增加特定元素倍率，如三个\"七七\"等。\n投币失败，已经是最高注。";
					 }
					 else {
						 laohuji[0]++;//加注数
						 _7fang[player_id].jinbi -= 2;
						 if (player_id == laohuji[1]) {
							 return "加注成功！";
						 }
						 else if (player_id != laohuji[1]) {
							 std::string a = "您已成功为" + tostring(_7fang[player_id].id) + "号id的7房大佬加注！";
							 return a;
						 }
					 }
				 }
			 case (3):
				 if (hear.find("投币") != std::string::npos || hear.find("加注") != std::string::npos) {
					 if (laohuji[0] == 3 || laohuji[0] > 3) {
						 return "最高注为三注，三注：三倍率，同时三横行两斜杠之任一行相同即中，多行中多重倍率，同时增加特定元素的倍率，如三个\"七七\",无百变三个相同。\n投币失败，已经是最高注。";
					 }
					 return "error";
				 }

				 if (hear.find("摇摇杆") != std::string::npos) {

					 //摇开哪个，默认全摇开
					 if (hear.find("摇开") != std::string::npos) {

						 if (hear.find("1") != std::string::npos) {
							 if (laohuji[4] == 1) {
								 return "你已经摇开第1个了，不准卡bug";
							 }
							 laohuji[4] += 1;
							 if (hear.find("2") != std::string::npos) {
								 if (laohuji[4] % 100 > 19) {
									 return "你已经摇开第2个了，不准卡bug";
								 }
								 laohuji[4] += 20;

								 if (hear.find("3") != std::string::npos) {
									 if (laohuji[4] > 99) {
										 return "你已经摇开第3个了，不准卡bug";
									 }
									 laohuji[4] += 100;
									 laohuji[3] = 0;
								 }
								 else { laohuji[3] = 4; }

							 }
							 else if (hear.find("3") != std::string::npos) {
								 if (laohuji[4] > 99) {
									 return "你已经摇开第3个了，不准卡bug";
								 }
								 laohuji[4] += 100;
								 laohuji[3] = 6;
							 }
							 else {
								 laohuji[3] = 1;
							 }
						 }
						 else if (hear.find("2") != std::string::npos) {
							 if (laohuji[4] % 100 > 19) {
								 return "你已经摇开第2个了，不准卡bug";
							 }
							 laohuji[4] += 20;
							 if (hear.find("3") != std::string::npos) {
								 if (laohuji[4] > 99) {
									 return "你已经摇开第3个了，不准卡bug";
								 }
								 laohuji[4] += 100;
								 laohuji[3] = 5;
							 }
							 else {
								 laohuji[3] = 2;
							 }
						 }
						 else if (hear.find("3") != std::string::npos) {
							 if (laohuji[4] > 99) {
								 return "你已经摇开第3个了，不准卡bug";
							 }
							 laohuji[4] += 100;
							 laohuji[3] = 3;
						 }
						 else {//默认，全摇开
							 laohuji[3] = 0;
						 }
					 }
					 else {//默认，全摇开
						 laohuji[3] = 0;
					 }

					 //怎么摇
					 if (hear.find("地") != std::string::npos) {
						 if (hear.find("轻") != std::string::npos) {
							 laohuji[2] = 0;
						 }
						 else if (hear.find("柔") != std::string::npos) {
							 laohuji[2] = 1;
						 }
						 else if (hear.find("颤") != std::string::npos) {
							 laohuji[2] = 2;
						 }
						 else if (hear.find("剧") != std::string::npos) {
							 laohuji[2] = 3;
						 }
						 else if (hear.find("大力") != std::string::npos) {
							 laohuji[2] = 4;
						 }
						 else if (hear.find("狂") != std::string::npos) {
							 laohuji[2] = 5;
						 }
						 else if (hear.find("欢") != std::string::npos) {
							 laohuji[2] = 6;
						 }
						 else if (hear.find("随") != std::string::npos) {
							 laohuji[2] = 7;
						 }

					 }

					 return yao_laohuji(_7fang,(int)laohuji[3], laohuji[2], laohuji[0], player_id);
				 }

				 break;
			 default:
				 youxi_clear();
				 std::cout << "[???] wei[1] is out of size\n";
				 return "这里必不可能出bug";
			 }

		 default:
			 youxi_clear();
			 std::cout << "[error] wei[0] is over the size\n";
			 return "error";
		 }//第一个switch
	 }//while
 }

 void xiling_youxi::youxi_clear() {

	 laohuji[0] = 0; laohuji[1] = -1;
	 laohuji[3] = 0; laohuji[2] = 10;
	 wei[0] = 0; wei[1] = 0;
	 zhadan = -1; f_caishu = -1;
	 for (int i = 0; i < 9; i++) {
		 yuansu[i] = gundong;
	 }
	 beilv = 0;
 }


 int xiling_zhuangtai::through_shanghai(std::vector<xiling_player>& _7fang, short userid,short useto, int liang,std::string& out_pangbai)
 {
	 liang = xiling_zhuangtai::zhuangtai_shanghai(_7fang, userid, useto, liang,out_pangbai);

	 return liang;
 }

 inline int xiling_zhuangtai::zhuangtai_shanghai(std::vector<xiling_player>& _7fang,short userid,short useto,int liang, std::string& out_pangbai)
 {
	 if (liang == -1) {
		 return liang;
	}
	
		 //造成伤害者 的状态遍历
		 for (zhuangtai& userone : _7fang[userid].buff) {
			 if (liang == -1 || liang == 0) {
				 break;
			 }
			 switch (userone) {
			 case die:			liang = 0; 
				 out_pangbai += _7fang[userid].name + "已经死亡，无法造成伤害。\n";
				 break;
			 case alive:
				 break;
			 case hunshui:		liang = 0; 
				 out_pangbai += _7fang[userid].name + "处于昏睡状态，造成物理伤害 0 点，但在梦里造成100000点伤害。\n";
				 break;
			 case daliwan:		liang = liang + _7fang[useto].health / 10 + 5; 
				 out_pangbai += _7fang[userid].name + "肌肉突然爆棚，大力丸额外效果成功生效。\n";
				 break;
			 case xuruo:		liang = liang / 2; 
				 out_pangbai += _7fang[userid].name + "感到浑身无力，虚弱效果使造成的伤害大幅降低。\n";
				 break;
			 case meihuo_acceptance:
				 for (auto value : _7fang[userid].buff_acceptance) {
					 if (value.m_buff==meihuo_acceptance && value.m_attack == &_7fang[useto]) {
						 liang = 0;
						 out_pangbai += _7fang[userid].name + "看到攻击目标，觉得对方超好看！不自觉地收回了攻击。\n";
						 break;
					 }
				 }
			 case meihuo_attack:
				 break;
			 case yinluan:		liang = 0;
				 out_pangbai += _7fang[userid].name + "觉得自己脑子有点不对劲，将要发出的攻击突然颤动，脸上浮现一片潮红，攻击全部散开失效。\n";
				 break;
			 case zhongdu:		
				 break;
			 case bulaobusi:	liang = 0;
				 out_pangbai += _7fang[userid].name + "佛心盛起，实在不忍心下手，于是收回了攻击。\n";
				 break;
			 default:
				 out_pangbai += "发生未知错误，详见控制台\n"+defaultlog();
				 _log<errorlevel>(error, "状态-伤害 列表越界 (可能是眩晕等状态，特殊越过了筛选成功发动)" + defaultlog()).m_log();
				 return -1;
			 }
		 }
		 if (liang == 0) {
			 return liang;
		 }
		 //收到伤害者 的状态遍历
		 for (zhuangtai& usetoone : _7fang[useto].buff) {
			 if (liang == -1 || liang == 0) {
				 break;
			 }
			 switch (usetoone) {
			 case die:	liang = 0; 
				 out_pangbai += _7fang[useto].name + "已经死亡，伤害强制为 0， 但选中成功，造成鞭尸效果。\n";
				 break;
			 case alive:
				 break;
			 case hunshui:
				 break;
			 case daliwan:
				 break;
			 case xuruo:
				 break;
			 case meihuo_acceptance:
				 break;
			 case meihuo_attack:
				 for (auto value : _7fang[useto].buff_attack) {
					 if (value.m_buff == meihuo_attack && value.m_acceptance == &_7fang[userid]) {
						 liang = 0;
						 out_pangbai += _7fang[userid].name + "看到攻击目标，觉得对方超好看！不自觉地收回了攻击。\n";
						 break;
					 }
				 }
				 break;
			 case yinluan: liang += 5;
				 out_pangbai += _7fang[useto].name+"因为淫乱状态没有成功组织起简单防御，受到伤害 + 5\n";
				 break;
			 case xuanyun:
				 break;
			 case zhongdu:
				 break;
			 case bulaobusi:liang = 0;
				 out_pangbai += _7fang[useto].name + "见状大喊阿弥陀佛并使出了金钟罩，完全抵御下了攻击。\n";
				 break;

			 default:
				 out_pangbai += "发生未知错误，详见控制台\n" + defaultlog();
				 _log<errorlevel>(error, "状态-伤害 列表越界 " + defaultlog()).m_log();
				 return -1;
			 }
		 }
		 return liang;
		// _log<errorlevel>(error, "状态-伤害 未知错误 " + defaultlog()).m_log();
	 //return -1;
 }
