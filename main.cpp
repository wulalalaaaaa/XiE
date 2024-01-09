
// MiraiCP依赖文件(只需要引入这一个)
#include <MiraiCP.hpp>
#include<ctime>
#include<memory>
#include<array>
#include "_log.h"
#include "basetools.h"
#include "xiling_player.h"
#include "xiling_wupin.h"
#include "xiling_jineng.h"
#include "xiling_npc.h"

#include<Windows.h>

//#define log(x) std::cout<<x<<std::endl

using namespace MiraiCP;

const PluginConfig CPPPlugin::config{
        "114514.114514",          // 插件id
        "little_xiling",        // 插件名称
        "0.1",            // 插件版本
        "xiling",        // 插件作者
        "cute，内斥暴虐烈火的上古失神之灵，离开极地后为了能量平衡，平时化身为小灵体形态 ~ "			 // 可选：插件描述
        "2077.13.97"        // 可选：日期
};

//xiling_yuyan组
#pragma region xiling_yuyan
std::string yuyan_chuli(std::string& send, const std::string& bian);
int _3FindQian(const std::string& a);
int _2FindHou(const std::string& send);
int stringHave(std::string jieshou, const std::string& beiti);
bool Is_ask(const std::string& a, const std::string& foudingyu = "不否非没弗");
int xi = 0;
#pragma endregion

//档案组
#pragma region xiling_player
short IDMAX = 0;
std::vector<xiling_player> _7fang;
short find_id(QQID userid, GroupMessageEvent& hear);
#pragma endregion

//水晶球类
std::array<xiling_npc, 4>zhujuetuan_4;
int num_zhujuetuan = 0;//当前主角团人数
std::vector<xiling_npc>npc;
int juqing_weizhi = 0;

//物品组
std::string wupin_send(std::string_view hear, std::vector<xiling_player>& _7fang, short userid);
																					//状态检测位置：wupin_send函数体开头，CanFadong函数体，			

//技能组
std::vector<jineng>alljineng;



bool CanFadong(std::vector<xiling_player>& _7fang, short userid);
//技能组

//商店！
std::vector<wupin>shangdian;
std::string chouka(std::vector<xiling_player>& _7fang, short id);
std::string chouka_10(std::vector<xiling_player>& _7fang, short id);
inline void pushin(xiling_player& one, int suijishu);
inline std::string showchouka(xiling_player& one, std::unique_ptr<wupin>& wupin);
inline void showchouka_10(xiling_player& one, std::unique_ptr<wupin>& wupin, std::string& shuchu);


//之前的消息缓存下来
struct huancun {
    std::string m_msg;
    MiraiCP::QQID m_id;

    huancun()
        :m_msg(""),m_id(0)
    {}
    huancun(const std::string& msg,const MiraiCP::QQID& id)
        :m_msg(msg),m_id(id)
    {}
    void operator()(const std::string& msg, const MiraiCP::QQID& id) {
        this->m_msg = msg;
        this->m_id = id;
    }
    bool operator==(const huancun& other)const {
        if (m_msg == other.m_msg && m_id != other.m_id) {
            return 1;
        }
        return 0;
    }
};
//大消息链缓存
//struct bighuancun : public huancun {
//	
//private:
//	MessageChain m_msgChain;
//	
//public:
//	Image m_tupian;
//
//	
//	bighuancun()
//		:huancun(), m_msgChain(""), m_tupian(m_msgChain.filter<Image>()[0]) {}
//	bighuancun(const std::string& msg, const MiraiCP::QQID& id,const MessageChain& msgChain)
//		:huancun(msg,id), m_msgChain(msgChain), m_tupian(m_msgChain.filter<Image>()[0]){}
//
//	bool operator==(const bighuancun& other)const {
//		if (m_msgChain == other.m_msgChain)return 1;
//		return 0;
//	}
//
//};

//上一个文本消息，上上一个文本消息
huancun huancun1; huancun huancun2;
MessageChain* all_huancun1; MessageChain* all_huancun2;
unsigned long long Member_huancun1(2416314577);
unsigned long long Member_huancun2(2416314577);

//0：无效果，1：闪光弹效果，
int main_fadong_flag = 0;
//0：无效果，1：1号水晶球，副本准备状态，主角团选人状态，2:2号水晶球准备状态。
int main_shuijinqiu_zhunbei = 0;
//0：无人处于副本，1：处于编号1的副本，2：处于编号2的副本，等等
//int main_fuben_flag = 0;

#define JINENGSHU 13
#define WUPINSHU 27

// 插件实例
class Main : public CPPPlugin {
public:
  // 配置插件信息
  Main() : CPPPlugin() {

	  _7fang.reserve(20);
	  _7fang.emplace_back(IDMAX, 1302349577, "**星", 2000, 5000, 233333,50,150,100,100);
	  _7fang.emplace_back(IDMAX, 2416314577, "**", 100, 110, 100);
	  /*_7fang.emplace_back(IDMAX, 956913166, "阿符", 100, 110, 100); 
	  _7fang.emplace_back(IDMAX, 3417187063, "丹朱", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 3056989455, "无尽", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 1836837800, "胡桃", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 2100532794, "初雨", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 1248063219, "豆", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 3062563014, "苓", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 1307977565, "蓝毛", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 3248339803, "千辉", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 822943938, "夕", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 3121546208, "帽砸", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 2543792170, "玖", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 1295764175, "璃晓", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 1254755503, "风纪委员", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 458944782, "浅灰", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 1433523153, "安之", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 1431414320, "Ineffable", 100, 110, 100);
	  _7fang.emplace_back(IDMAX, 3340948185, "幻想的境界", 100, 110, 100);*/
	  
	  //商店の菜单！
	  shangdian.reserve(WUPINSHU);
	  shangdian.emplace_back("苹果", 101, white, &_7fang[0]);
	  shangdian.emplace_back("奇怪的球", 102, blue,& _7fang[0],10);
	  shangdian.emplace_back("水枪", 103, blue, &_7fang[0],10);


	  shangdian.emplace_back("小药瓶", 201, white,& _7fang[0]);
	  shangdian.emplace_back("中药瓶",202,blue, &_7fang[0]);
	  shangdian.emplace_back("大药瓶",203,purple,& _7fang[0]);
	  shangdian.emplace_back("小魔力瓶",204,white,& _7fang[0]);
	  shangdian.emplace_back("中魔力瓶",205,blue, &_7fang[0]);
	  shangdian.emplace_back("大魔力瓶",206,purple,& _7fang[0]);
	  shangdian.emplace_back("大力丸",207,purple, &_7fang[0]);
	  shangdian.emplace_back("虚弱剂",208,purple, &_7fang[0]);
	  shangdian.emplace_back("电脑配件",209,golden,& _7fang[0]);
	  shangdian.emplace_back("复生丹",210,golden, &_7fang[0]);
	  shangdian.emplace_back("唐僧肉",211,golden, &_7fang[0]);
	  shangdian.emplace_back("红茶",212,golden, &_7fang[0]);
	  shangdian.emplace_back("小毒丸",213,purple, &_7fang[0]);
	  shangdian.emplace_back("解毒丸",214,purple, &_7fang[0]);
	  
	  shangdian.emplace_back("《换手拿球》",414,white, &_7fang[0]);
	  shangdian.emplace_back("《治疗术》",407,blue, &_7fang[0]);
	  shangdian.emplace_back("《冰盾》",405,blue, &_7fang[0]);
	  shangdian.emplace_back("《火球术》",412,purple,& _7fang[0]);
	  shangdian.emplace_back("《挠挠挠》",403,blue, &_7fang[0]);
	  shangdian.emplace_back("《魅惑》",402,purple, &_7fang[0]);
	  shangdian.emplace_back("《重锤》",415,blue, &_7fang[0]);
	  shangdian.emplace_back("《闪光弹》",416,purple,& _7fang[0]);
	  shangdian.emplace_back("《暴虐》",417,golden, &_7fang[0]);



	  shangdian.emplace_back("灰不溜秋水晶球1号",501,golden, &_7fang[0]);





	  //技能の菜单！
	  alljineng.reserve(JINENGSHU);
	  alljineng.emplace_back("普通攻击", 101, white, nullptr, 30);//0
	  alljineng.emplace_back("魔力冲击", 102, white, nullptr, 30);//1

	  //alljineng.emplace_back("太虚剑神", 2, golden, nullptr, 120);//2
	  //alljineng.emplace_back("聚爆燃魂", 10, golden, nullptr, 120);//3

	  alljineng.emplace_back("换手拿球", 117, white, nullptr, 20);//4

	  alljineng.emplace_back("治疗术", 111, blue, nullptr, 60);//5
	  alljineng.emplace_back("冰盾", 108, blue, nullptr, 120);//6
	  alljineng.emplace_back("火球术", 115, purple, nullptr, 60);//7
	  alljineng.emplace_back("挠挠挠",106, blue, nullptr, 60);//8
	  alljineng.emplace_back("魅惑",105, purple, nullptr, 120);//9
	  alljineng.emplace_back("重锤",109, blue, nullptr, 60);//10
	  alljineng.emplace_back("闪光弹",110, purple, nullptr, 120);//11
	  alljineng.emplace_back("暴虐",118, golden, nullptr, 140);//12
	  

	  //初始化普通攻击和魔力冲击
	  for (int i = 0; i < _7fang.size(); i++) {
		  _7fang[i].jinengbiao.reserve(3);
		  _7fang[i].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[0],&_7fang[i]));
		  _7fang[i].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[1],&_7fang[i]));
	  }

	  //初始化专属技能
	 /* _7fang[2].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[2], &_7fang[2]));
	  _7fang[10].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[3], &_7fang[10]));*/


  }
  ~Main() override = default;

  // 入口函数
  void onEnable() override {
    // 请在此处监听

			/* Contact a(1,2416314577,0,"多功能点歌姬",1302349577,false);
			a.sendMessage(PlainText("hello!"));*/
	  
	 
		 // switch (main_fuben_flag) {
			//case 0://日常功能
		Event::registerEvent<GroupMessageEvent>([](GroupMessageEvent hear) {
			std::vector<PlainText> hearPlainText = hear.getMessageChain()->filter<PlainText>();
			if (!hearPlainText.empty()) {
              std::vector<std::string>hearString;
              hearString.reserve(3);
              for (int i = 0; i < hearPlainText.size(); i++) {
                  hearString.emplace_back(hearPlainText[i].content);
              }

              huancun2 = huancun1; huancun1(hearString[0], hear.sender.id());

			  if (hearString[0].length() > (unsigned)5) {

				  std::string_view hears(hearString[0].c_str(), 6);
				  if (hears == "**") {
					  std::string shuchu = yuyan_chuli(hearString[0], "**");
					  hear.group.sendMessage(PlainText(shuchu));

				  }
				  else if (hears == "游戏") {
					  std::string shuchu = xiling_youxi::send(_7fang, hearString[0], find_id(hear.sender.id(),hear));
					  hear.group.sendMessage(PlainText(shuchu));

				  }
				  else if (hears == "使用") {
					  if (hearString[0].size() > 6) {
						  std::string_view use(hearString[0].c_str() + 6, hearString[0].size() - 6);
						  std::string shuchu = wupin_send(use, _7fang, find_id(hear.sender.id(),hear));
						  hear.group.sendMessage(PlainText(shuchu));
					  }
					  else {
						  hear.group.sendMessage(PlainText("这样！：使用+物品，或：使用+物品+[空格]+使用对象"));
					  }
				  }
				  else if (hears == "属性") {
					  if (hearString[0].find(" ") != -1) {
						  if (hearString[0].size() > 11) {
							  std::string_view hears1(hearString[0].c_str() + 6, 6);
							  if (hears1 == "背包") {
								  QQID useto_qq = getint<QQID>(hearString[0]);
								  hear.group.sendMessage(PlainText(_7fang[find_id(hear.sender.id(), hear)].show_beibao(useto_qq)));
							  }
							  else if(hears1 =="技能") {
								  QQID useto_qq = getint<QQID>(hearString[0]);
								  hear.group.sendMessage(PlainText(_7fang[find_id(hear.sender.id(), hear)].show_jineng(useto_qq)));
							  }
						  }
						  std::string useto_qq = hearString[0].substr(7, hearString[0].size() - 7);
						  QQID qq02 = getint<QQID>(useto_qq);
						  if (qq02 > 1000) {
							  int i = 0;
							  for (; i < _7fang.size(); i++) {
								  if (_7fang[i].qq == qq02) {
									  hear.group.sendMessage(PlainText(_7fang[i].show()));
									  break;
								  }
							  }
							  if (i == _7fang.size()) {
								  hear.group.sendMessage(PlainText("没有找到这个人的档案，请仔细检查输入的qq号是否正确"));
							  }
						  }
						  else {
							  if ((int)qq02 < (int)_7fang.size()) {
								  hear.group.sendMessage(PlainText(_7fang[qq02].show()));
							  }
							  else {
								  hear.group.sendMessage(PlainText("没有找到这个人的档案，请仔细检查输入的id/qq是否正确"));
							  }
						  }
					  }
					  //改名字被划分到技能里了
					  /*	  if(hearString[0].length()>(unsigned)7){
							if (hearString[0].substr(7, 12) == "改名") {

								if (hearString[0].length() > (unsigned)60) {
									hear.group.sendMessage(PlainText("你的名字也太长了，这可不行哦（歪指"));
								}
								else {
									_7fang[id].magic -= 10;
									_7fang[id].setname(hearString[0].substr(13, hearString[0].length()));
									hear.group.sendMessage(PlainText("名字改好啦~您的新名字是" + hearString[0].substr(13, hearString[0].length())));
								}
							}*/
					  else {
						  hear.group.sendMessage(PlainText(_7fang[find_id(hear.sender.id(), hear)].show()));
					  }
				  }
				  else if (hears == "商店") {
					  if (hearString[0].size() > 11) {
						  std::string_view hears1(hearString[0].c_str() + 6, 6);
						  if (hears1 == "抽卡") {
							  if (_7fang[find_id(hear.sender.id(), hear)].jinbi < 3) {
								  hear.group.sendMessage(PlainText("你没钱啦！投掷一次幸运骰子并有两人认可结果后，可以获得50金币哦 ~ （暂时没做‘两人认可’，但投骰子并做到之后，我可以后台给金币）"));
							  }
							  else {
								  _7fang[find_id(hear.sender.id(), hear)].jinbi -= 3;
								  std::string shuchu = chouka(_7fang, find_id(hear.sender.id(), hear));
								  hear.group.sendMessage(PlainText(shuchu));
							  }
						  }
							  if (hearString[0].size() > 14) {
								  std::string_view hears2(hearString[0].c_str() + 6, 9);
								  if (hears2 == "十连抽") {
									  if (_7fang[find_id(hear.sender.id(), hear)].jinbi < 30) {
										  hear.group.sendMessage(PlainText("你没钱啦！投掷一次幸运骰子并有两人认可结果后，可以获得50金币哦 ~ （暂时没做‘两人认可’，但投骰子并做到之后，我可以后台给金币）"));
									  }
									  else {
										  _7fang[find_id(hear.sender.id(), hear)].jinbi -= 30;
										  std::string shuchu = chouka_10(_7fang, find_id(hear.sender.id(), hear));
										  hear.group.sendMessage(PlainText(shuchu));
									  }
								  }
							  }
					  }
				  }
				  else if(hears =="发动") {
					  short userid= find_id(hear.sender.id(), hear);
					  if (CanFadong(_7fang, userid)) {
						  std::string shuchu;
						  if (hearString[0].size() > 8) {
						  int xuanzhong = hearString[0].find(" "); 
						  if (xuanzhong == -1) {
							 
								  std::string_view jinengming(hearString[0].c_str() + 6, hearString[0].size() - 6);
								  for (xuanzhong = 0; xuanzhong < _7fang[userid].jinengbiao.size(); xuanzhong++) {
									  if (_7fang[userid].jinengbiao[xuanzhong]->m_name == jinengming) {
										  break;
									  }
								  }
								  if (xuanzhong == _7fang[userid].jinengbiao.size()) {
										shuchu = "请这样说:\"发动\"+\"技能名字\",,或:\"发动\"+\"技能名字\"+ 空格 +\"选中目标的名字\"";
								  }
								  else {
									  if (_7fang[userid].jinengbiao[xuanzhong]->m_incd) {
										  shuchu = "技能冷却中。";
									  }
									  else {
										  //条件达成，技能发动：
										  int useto=0;
										  switch (main_fadong_flag) {
										  case 0:
											  shuchu = _7fang[userid].jinengbiao[xuanzhong]->send(_7fang[userid].jinengbiao[xuanzhong]->find_jineng_fadong1(_7fang[userid].jinengbiao[xuanzhong]->m_bianhao), _7fang, userid);
											  _7fang[userid].jinengbiao[xuanzhong]->incd();
											  break;
										  case 1:
											  srand((unsigned)time(NULL));
											  useto = rand() % _7fang.size();
											  shuchu = "由于被闪瞎了眼睛，此次技能打在了" + _7fang[useto].name + "的身上！\n";
											  shuchu += _7fang[userid].jinengbiao[xuanzhong]->send(_7fang[userid].jinengbiao[xuanzhong]->find_jineng_fadong2(_7fang[userid].jinengbiao[xuanzhong]->m_bianhao), _7fang, userid,useto);
											  _7fang[userid].jinengbiao[xuanzhong]->incd();
											  main_fadong_flag = 0;
											  break;
										  default:
											  shuchu = "[error] flag oversize.";
											  main_fadong_flag = 0;
											  break;
										  }
										  
									  }
								  }
						  }		
						  else {
							  std::string_view onejineng(hearString[0].c_str()+6, xuanzhong-6);
							  std::string_view useto(hearString[0].c_str()+xuanzhong+1, hearString[0].size() - xuanzhong-1 );
							  for (xuanzhong = 0; xuanzhong < _7fang[userid].jinengbiao.size(); xuanzhong++) {
								  if (_7fang[userid].jinengbiao[xuanzhong]->m_name == onejineng) {
									  break;
								  }
							  }
							  if (xuanzhong == _7fang[userid].jinengbiao.size()) {
									shuchu = "请这样说:\"发动\"+\"技能名字\",,或:\"发动\"+\"技能名字\"+ 空格 +\"选中目标的名字\"";
							  }
							  else {
								  short usetoid = 0;
								  for (; usetoid < _7fang.size(); usetoid++) {
									  if (_7fang[usetoid].name == useto) {
										  break;
									  }
								  }
								  if (usetoid == _7fang.size()) {
									  shuchu = "请这样说:\"发动\"+\"技能名字\",,或:\"发动\"+\"技能名字\"+ 空格 +\"选中目标的名字\"";
								  }
								  else {
									  if (_7fang[userid].jinengbiao[xuanzhong]->m_incd) {
										  shuchu = "技能冷却中。";
									  }
									  else {
										  //条件达成，技能发动：
										  
										  switch (main_fadong_flag) {
										  case 0: 
											  shuchu = _7fang[userid].jinengbiao[xuanzhong]->send(_7fang[userid].jinengbiao[xuanzhong]->find_jineng_fadong2(_7fang[userid].jinengbiao[xuanzhong]->m_bianhao), _7fang, userid, usetoid);
											  _7fang[userid].jinengbiao[xuanzhong]->incd();
											  break;
										  case 1:
											  srand((unsigned)time(NULL));
											  usetoid = rand() % _7fang.size();
											  shuchu = "由于被闪瞎了眼睛，此次技能打在了" + _7fang[usetoid].name + "的身上！\n";
											  shuchu += _7fang[userid].jinengbiao[xuanzhong]->send(_7fang[userid].jinengbiao[xuanzhong]->find_jineng_fadong2(_7fang[userid].jinengbiao[xuanzhong]->m_bianhao), _7fang, userid, usetoid);
											  _7fang[userid].jinengbiao[xuanzhong]->incd();
											  main_fadong_flag = 0;
											  break;
										  default:
											  shuchu = "[error] flag oversize.";
											  main_fadong_flag = 0;
											  break;
										  }
										 
									  }
								  }
							  }
						  }
						  hear.group.sendMessage(PlainText(shuchu));
					  }
						  else {
							  hear.group.sendMessage(PlainText("请这样说:\"发动\"+\"技能名字\",,或:\"发动\"+\"技能名字\"+ 空格 +\"选中目标的名字\""));
						  }
					  }
					  else {
						  hear.group.sendMessage(PlainText("处于异常状态中，发动失败。"));
					  }
				  }
				  else if(hears =="重置") {
						if (hearString[0].substr(6, 6) == "全部") {
							for (xiling_player& value : _7fang) {
								value.clear();
							}
							xiling_youxi::youxi_clear();
							_log<errorlevel>(green, "已成功重置所有信息。").m_log();
						 }
						else if (hear.sender.id() == 2416314577) {
							if (hearString[0].find("金币") == -1) {
								QQID useto = getint<QQID>(hearString[0]);
								short id = find_id(useto, hear);
								_7fang[id].clear();

								_7fang[id].jinengbiao.reserve(3);
								_7fang[id].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[0], &_7fang[id]));
								_7fang[id].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[1], &_7fang[id]));
								
								_log<errorlevel>(green, "已成功重置这个人。").m_log();
							}
							else {
								QQID useto = getint<QQID>(hearString[0]);
								short id = find_id(useto, hear);
								_7fang[id].jinbi += 50;
							}
						}
				  }
				  else {
				  switch (main_shuijinqiu_zhunbei) {
				  case 0:
					  break;
				  case 1://1号水晶球的准备状态；
					  if (hears == "加入") {
						  if (num_zhujuetuan == 4) {
							  hear.group.sendMessage(PlainText("主角团人数已满，可开启后附身控制npc干扰主角团正常剧情走向。"));
						  }
						  else {
							  std::string shuchu;
							  int xuanzhong = getint<int>(hearString[0]);
							  switch (xuanzhong) {
							  case 1:if (zhujuetuan_4[0].setPlayer(&_7fang[find_id(hear.sender.id(), hear)])) {
										shuchu = "加入成功。";
									}
									else {
										shuchu = "此角色已被占据，加入失败。";
									}
								  break;
							  case 2:if (zhujuetuan_4[1].setPlayer(&_7fang[find_id(hear.sender.id(), hear)])) {
										  shuchu = "加入成功。";
									  }
									else {
										  shuchu = "此角色已被占据，加入失败。";
									  }
								  break;
							  case 3:if (zhujuetuan_4[2].setPlayer(&_7fang[find_id(hear.sender.id(), hear)])) {
										  shuchu = "加入成功。";
									  }
									else {
										  shuchu = "此角色已被占据，加入失败。";
									  }
								  break;
							  case 4:if (zhujuetuan_4[3].setPlayer(&_7fang[find_id(hear.sender.id(), hear)])) {
										  shuchu = "加入成功。";
									  }
									else {
										  shuchu = "此角色已被占据，加入失败。";
									  }
								  break;
							  default:
								  shuchu = "请附加想要加入的主角团的编号，如“加入2”。主角团：\n";
								  shuchu += "1:  "+zhujuetuan_4[0].showall();
								  shuchu += "2:  "+zhujuetuan_4[1].showall();
								  shuchu += "3:  "+zhujuetuan_4[2].showall();
								  shuchu += "4:  "+zhujuetuan_4[3].showall();

								  _log<errorlevel>(okBut, "(可能)主角团加入选中越界。").m_log();
								  break;
							  }
						  }
						  //xiling_player zhujue(userid, _7fang[userid].qq, _7fang[userid].name, 200, 120, 50, 10, 10, 20, 20);
						  //zhujuetuan_4[num_zhujuetuan++] = zhujue;
					  }
					  else if (hears == "开启") {
						  if (num_zhujuetuan) {
							  std::string shuchu;
							  shuchu = "水晶球发出耀眼的灰不溜秋的光芒";
							  while (num_zhujuetuan--) {
								  shuchu +=","+ zhujuetuan_4[num_zhujuetuan].name();
							  }
							  shuchu += "被卷入水晶球中。秘境期间所有人无法与外界取得联系，非主角团可以附身控制npc，最终结局决定所有人的金币收益，本水晶球：结局越好主角团金币越多，结局越混乱非主角团金币越多。";

							  hear.group.sendMessage(PlainText(shuchu));

							  Sleep(5000);
							  shuchu = "几人渐渐缓过来，一行人都躺在一片草地上，紫色的太阴明媚生姿，小草与泥土的芬芳拌着“嘎!嘎!嘎啦！”的鸟鸣声真让人心旷神怡。四下望去，东面的远处有一片森林，西面有一个小镇，北面有空气墙，南面是悬崖。\n那么，要去哪边？";
							  hear.group.sendMessage(PlainText(shuchu));
							  while (1) {
								  std::string hears = hear.nextMessage(300000).filter<PlainText>()[0].content;
								  if (hears == "东") {
									  hear.group.sendMessage(PlainText("成功进入 分支——东！"));
									  break;
								  }
								  else if (hears == "西") {
									  break;
								  }
								  else if (hears == "南") {
									  break;
								  }
								  else if (hears == "exit") {
									  break;
								  }
							  }
							  /*int main_fuben_flag = 1;*/
						  }
					  }

					  main_shuijinqiu_zhunbei = 0;
					  break;
				  case 2:
					  main_shuijinqiu_zhunbei = 0;
					  break;
				  default:
					  hear.group.sendMessage(PlainText("进入奇怪的分支 default，"));
					  main_shuijinqiu_zhunbei = 0;
					  break;
				  }

				  }
			  }
              if (hearString.size() > 1) {
				  std::string_view hears(hearString[0].c_str(), 6);
				  if (hears == "**" || hears == "发动" || hears == "使用") {
					  hear.group.sendMessage(PlainText("不好意思喵，同一个消息本灵只回答第一条文字信息~"));
				  }
              }
          }
			/*if (*all_huancun2 == *all_huancun1 && *all_huancun1 == *hear.getMessageChain() && huancun1.m_msg != "商店抽卡" && huancun1.m_msg != "商店十连抽") {
			  hear.group.sendMessage(*hear.getMessageChain());
			}*/

			//all_huancun2 = all_huancun1;
			//all_huancun1 = hear.getMessageChain();
		});
		/*	break;*/

			//case 1://1号水晶球秘境内功能
			//	Event::registerEvent<GroupMessageEvent>([](GroupMessageEvent hear) {
			//		std::vector<PlainText> hearPlainText = hear.getMessageChain()->filter<PlainText>();
			//		std::string hearString = hearPlainText[0].toMiraiCode();
			//		std::string_view hears(hearString.c_str(), 6);
			//		if (hears == "**" || hears == "商店") {
			//			hear.group.sendMessage(PlainText("处于特殊秘境「有点脏的灰水晶球」，无法接触外界商店或联系外界小**。"));
			//		}
			//		else {
			//			if (hears == "附身") {
			//				
			//			}
			//			else if(hears == "使用") {

			//			}
			//			else if (hears == "发动") {

			//			}




			//		}


			//		});
			//	break;

			//default:
			//	_log<errorlevel>(error, "副本进入编号越界！——————————").m_log();
			//	break;
			//}

		//戳一戳
		Event::registerEvent<NudgeEvent>([](NudgeEvent hear) {
			Member a(hear.from.id(), hear.from.groupid(), 1302349577);
			Group qun(hear.from.groupid(), 1302349577);
			if (hear.from.id() != 1302349577) {
				if(hear.target.id()== 1302349577){
					srand((unsigned)time(NULL));
					if (a.id() == Member_huancun1) {
						MessageChain msg;
						switch (rand() % 3) {
						case 0:
							msg.add(PlainText("哼！\n"));
							msg.add(Image("E:\\justborrow\\Tu\\_emotion\\敲\\qiao.jpg", 12288, 250, 137, 2));
							qun.sendMessage(msg);
							break;
						case 1:
							qun.sendMessage(PlainText("再戳我真要咬人了...！"));
							break;
						case 2:
							/*msg.add(PlainText("啊呜！\n"));*/
							msg.add(Image("E:\\justborrow\\Tu\\_emotion\\敲\\nie.jpg", 110592, 1036, 1073, 2));
							qun.sendMessage();
							break;
						default:
							break;

					}

				}

				}
				else {
					switch (rand() % 6) {
					case 0:
						qun.quoteAndSendMessage(PlainText("再，再戳要咬人了！"), MessageSource());
						break;
					case 1:
						qun.sendMessage(PlainText("别戳了..."));
						break;
					case 2:
						a.sendNudge();
						break;
					case 3:
						qun.sendMessage(Image("E:\\justborrow\\Tu\\_emotion\\敲\\wu.jpg", 24576,324,266,2));
						break;
					case 4:
						qun.sendMessage(Image("E:\\justborrow\\Tu\\_emotion\\哼！\\heng.jpg", 28672, 119, 130, 2));
						break;
					case 5:
						qun.sendMessage(PlainText("再戳会坏掉的...!"));
					default:
						break;

					}
				}
				
				Member_huancun2 = Member_huancun1;
				Member_huancun1 = a.id();
			}
			  });

		
			



  }

  // 退出函数
  void onDisable() override {
    /*插件结束前执行*/

  }
};

// 绑定当前插件实例
void MiraiCP::enrollPlugin() {
  MiraiCP::enrollPlugin(new Main);
}


#pragma region d对话模块的函数


/// <summary>
/// 一定先确保新接收信息不等于huancun1的信息，求出现任意其中beiti字符的次数
/// </summary>
/// <param name="jieshou">接收的被判断方</param>
/// <param name="beiti">要判断的字符集</param>
/// <returns>被判断方中出现任意后者字符的次数</returns>
int stringHave(std::string jieshou, const std::string& beiti) {
    int a = 0;
    while (huancun1.m_msg != jieshou) {
        huancun1.m_msg = jieshou;
        jieshou = jieshou.substr(jieshou.find_first_of(beiti) + 1, jieshou.length() - jieshou.find_first_of(beiti));

        a++;
    }

    return (a - 1) / 2;
}

/// <summary>
/// 自然语言处理，作废状态，暂且放这，以后推翻重做，重做的想法更有思路，就不必事事亲为给每个情况都写回应了
/// </summary>
/// <param name="send">听到的，前提语（现在用"**"）</param>
/// <returns>说出的</returns>
std::string yuyan_chuli(std::string& send, const std::string& bian) {

	using namespace std;

	std::string shuchu("叫**干嘛呢！（凶@ ");

	srand((unsigned int)time(NULL));

	int foudingshu_bu = stringHave(send, "不否非没弗");
	if (Is_ask(send)) {
		//问句；
		shuchu = "**认得，这是疑问句！";
		return shuchu;
	}
	else if (send.find("是") != string::npos || send.find("会") != string::npos || send.find("想") != string::npos || send.find("要") != string::npos || send.find("好") != string::npos) {
		std::string hou = "";
		if (send.find("是") != string::npos) {

			const int shi_qian = _3FindQian(send);
			const int shi_hou = _2FindHou(send);

			if (foudingshu_bu % 2 == 0) {
				switch (shi_qian) {
				case(0): {
					switch (shi_hou) {
					case(0): {
						//00
						hou = send.erase(0, send.find("是"));
						switch (rand() % 2 + 1) {
						case(1):shuchu = "不是！**才不是" + hou;
							xi--;
							break;
						case(2):shuchu = "嗯！！你才是" + hou;
							xi -= 2;
							break;
						}}
						   break;
					case(1): {
						//01
						xi += 2;
						switch (rand() % 3 + 1) {
						case(1):	shuchu = "欸，**会不好意思的。。。";
							break;
						case(2):	shuchu = "呐~（是吧是吧~）";
							break;
						case(3):	shuchu = "诶嘿嘿...";
							break;
						}
					}
						   break;
					case(2):
						hou = send.erase(0, send.find("是"));
						shuchu = "**不明白" + hou + "是什么，但**觉得可以打你";
						break;
					default:
						shuchu = "error";
						break;
					}}
					   break;

				case(1): {
					switch (shi_hou) {
					case(0): {
						//10

						switch (rand() % 2 + 1) {
						case(1): shuchu = "为什么这样说呢，**惹你生气了吗";
							break;
						case(2):
							hou = send.erase(0, send.find(bian));
							shuchu = "hhhhhhhh**发现了" + hou + "!";
							xi++;
							break;
						}
					}
						   break;

					case(1): {
						//11
						hou = send.erase(0, send.find(bian));
						switch (rand() % 3 + 1) {
						case(1):shuchu = "好呢好呢,你真是好" + hou + "呢";
							break;
						case(2):shuchu = "有人自恋呢，我不说是谁";
							break;
						}
					}
						   break;
					case(2): {
						hou = send.erase(0, send.find(bian));
						shuchu = "**不明白" + hou + "是什么";

					}
						   break;
					}}
					   break;

				case(2): {
					switch (shi_hou) {
					case(0): {
						//020
						shuchu = "不许背后说别人坏话哦！";
					}
						   break;

					case(1): {
						//021
						hou = send.substr(3, send.find(bian));
						shuchu = hou + "是这样的";
					}
						   break;

					case(2): {
						hou = send.erase(0, send.find(bian));
						shuchu = "**不明白" + hou + "是什么....";

					}
						   break;
					default:
						break;
					}}
					   break;

				default:
					break;

				}
			}
			else {
				switch (shi_qian) {
				case(0): {
					switch (shi_hou) {
					case(0):
						//非00
						shuchu = "嗯！没错！";
						break;
					case(1): {
						//非01
						int suijishu = rand() % 2 + 1;
						switch (suijishu) {
						case(1):	shuchu = "呜。。。";
							break;
						case(2):	shuchu = "**要求相应的时空规则证据与四维以上等价矩阵实例，不然你就是错的！";
							break;
						}
					}
						   break;
					default:
						shuchu = "**不认识这个呢";
						break;
					}
				}
					   break;

				case(1): {
					switch (shi_hou) {
					case(0): {
						//非10
						int suijishu = rand() % 2 + 1;
						switch (suijishu) {
						case(1): shuchu = "为什么这样说呢，**惹你生气了吗";
							break;
						case(2):shuchu = "hhhhhhhh**发现了一只" /*+ "."*/;
							break;
						}
					}
						   break;

					case(1): {
						//非11
						shuchu = "好呢好呢";
					}
						   break;
					}}
					   break;

				case(2): {
					switch (shi_hou) {
					case(0): {
						//非20
						shuchu = "不许背后说别人坏话哦！";
					}
						   break;

					case(1): {
						//非21
						shuchu =/* "." +*/ "是这样的";
					}
						   break;

					case(2): {
						shuchu = "**不明白"/*" + "." + "*/"是什么....";

					}
						   break;
					}}
					   break;

				}
			}

		}
		else if (send.find("会") != string::npos) {

			const int hui_qian = _3FindQian(send);

			if (foudingshu_bu % 2 == 0) {
				switch (hui_qian)
				{
				case (0):shuchu = "**什么都会呢！";
					xi += 3;
					break;
				case(1):shuchu = "不也挺好的嘛";
					break;
				case(2): shuchu = "嗯嗯";
					break;
				default:shuchu = "error";
					break;
				}

			}
			else {
				switch (hui_qian) {
				case(0):shuchu = "嗯！！（甩头）**会努力学的！";
					break;
				case(1):shuchu = "要**教你吗？**可是天才哦";
					break;
				case(2):shuchu = "你又不是他你怎么知道它会不会" + send.erase(0, send.find("会")) + "呢！";
					break;
				default:shuchu = "error";
					break;
				}

			}



		}
		else if (send.find("想") < send.length()) {

			const int xiang_qian = _3FindQian(send);


			shuchu = "**认得，这是“想”！";


		}
		else if (send.find("要") < send.length()) {

			const int yao_qian = _3FindQian(send);

			shuchu = "**认得，这是“要”！";

		}
		else if (send.find("好") < send.length()) {

			const int hao_qian = _3FindQian(send);
			const int hao_hou = _2FindHou(send);

			shuchu = "**认得，这是“好”！";

		}
		else {
			shuchu = "[error] find key,but no key(这里不可能bug，怎么想都不可能";
		}
		return shuchu;
	}//谓词的认识末尾；
	//中间语部分；
	else {

		if (send.find("抱") != string::npos || send.find("摸") != string::npos || send.find("媷") != string::npos || send.find("吃") != string::npos || send.find("敲") != string::npos || send.find("揉") != string::npos) {
			const int suijishu = rand() % 3 + 1;

			switch (suijishu) {
			case(1):	shuchu = "你真的要" + send.erase(0, send.find(bian)) + "吗.....";
				xi += 2;
				break;
			case(2):	shuchu = "我觉得不行、";
				xi += 2;
				break;
			case(3):	shuchu = ".（无神.jpg）";
				xi += 5;
				break;
			default:    shuchu = "error";
				break;
			}

		}//抱摸媷敲揉吃、
		else if (send.find("摔") != string::npos || send.find("死") != string::npos || send.find("踢") != string::npos || send.find("扬") != string::npos || send.find("打") != string::npos || send.find("扔") != string::npos || send.find("拖") != string::npos || send.find("踩") != string::npos || send.find("滚") != string::npos || send.find("爬") != string::npos) {
			const int suijishu = rand() % 4 + 1;

			switch (suijishu) {
			case(1):	shuchu = "你真的要" + send.erase(0, send.find_first_of("摔刀扬死踢打看拖摇摆跑吵靠滑哭笑拿放扔捅踩屎滚爬")) + "吗.....";
				xi -= 1;
				break;
			case(2):	shuchu = ">、< **要求反击权";
				xi -= 2;
				break;
			case(3):	shuchu = "唔、为什么要这样对待**.....";
				xi -= 5;
				break;
			case(4):	shuchu = "再欺负**，**要找主人告状了！q、q";
				xi -= 3;
				break;
			default:    shuchu = "error";
				break;
			}
		}//摔刀扬死踢打看拖摇摆跑吵靠滑哭笑拿放扔捅踩屎滚爬、动词结束，下接形容词；
		else if (send.find("笨") != string::npos || send.find("垃圾") != string::npos || send.find("坏") != string::npos || send.find("傻") != string::npos) {
			const int suijishu = rand() % 4 + 1;
			switch (suijishu) {
			case(1):	shuchu = send.erase(0, send.find_first_of("垃圾坏笨傻")) + "!";
				xi -= 1;
				break;
			case(2):	shuchu = "(无神...)";
				xi -= 2;
				break;
			case(3):	shuchu = "唔、";
				xi -= 1;
				break;
			case(4):	shuchu = "q、q";
				xi -= 1;
				break;
			default:    shuchu = "error";
				break;
			}

		}//垃圾辣鸡差坏破笨傻、强可爱萌智慧好棒
		else if (send.find("强") != string::npos || send.find("可爱") != string::npos || send.find("萌") != string::npos || send.find("棒") != string::npos || send.find("耶") != string::npos || send.find("好") != string::npos || send.find("贴贴") != string::npos) {
			const int suijishu = rand() % 3 + 1;

			switch (suijishu) {
			case(1):	shuchu = "**也这么觉得！";
				xi += 2;
				break;
			case(2):	shuchu = ">_<";
				xi += 2;
				break;
			case(3):	shuchu = "QQ图片20211208165521";
				xi += 5;
				break;
			default:    shuchu = "error";
				break;


			}

		}


		return shuchu;
	}
}


/// <summary>
/// 找到主语
/// </summary>
/// <param name="send">处理原话</param>
/// <returns>返回主语判别数</returns>
int _3FindQian(const std::string& a) {

	//std::string c("他阿符蓝毛最弱妖精丹朱千夕初雨帽砸诱捕器胡桃酱幼狐玖  无尽 苓 ");
	if (a.find("你") != std::string::npos) {
		return 0;
	}
	else if (a.find("我") != std::string::npos) {
		return 1;
	}
	else if (a.find("他") != std::string::npos || a.find("阿符") != std::string::npos || a.find("蓝毛") != std::string::npos || a.find("丹朱") != std::string::npos || a.find("夕") != std::string::npos || a.find("初雨") != std::string::npos || a.find("胡桃") != std::string::npos || a.find("千辉") != std::string::npos || a.find("狐狸") != std::string::npos) {
		return 2;
	}
	else return 0;

}

/// <summary>
/// 只有两种反应方向、针对评价型原话
/// </summary>
/// <param name="send">处理原话</param>
/// <returns>返回倾向判别数</returns>
int _2FindHou(const std::string& send) {
	if (send.find("可爱") != std::string::npos || send.find("萌") != std::string::npos || send.find("耶") != std::string::npos || send.find("厉害") != std::string::npos || send.find("强") != std::string::npos || send.find("棒") != std::string::npos) {
		return 1;
	}
	else if (send.find("绿") != std::string::npos || send.find("熊") != std::string::npos || send.find("痴") != std::string::npos 
		|| send.find("废") != std::string::npos || send.find("呆") != std::string::npos || send.find("傻") != std::string::npos 
		|| send.find("障") != std::string::npos || send.find("机器") != std::string::npos || send.find("魔") != std::string::npos 
		|| send.find("鬼") != std::string::npos || send.find("坏") != std::string::npos || send.find("垃") != std::string::npos 
		|| send.find("差") != std::string::npos || send.find("破") != std::string::npos || send.find("笨") != std::string::npos 
		|| send.find("臭") != std::string::npos) {
		return 0;
	}
	else return 2;
	/*std::string a("可爱萌智慧厉害强好棒");
	std::string b("笨蛋绿熊白痴废傻呆滞障机器魔鬼坏");
	if (send.find_first_of(a) != string::npos) {
		return 1;
	}
	else if (send.find_first_of(b) != string::npos) {
		return 0;
	}
	else return 2;*/

}

/// <summary>
/// 是不是问句
/// </summary>
/// <param name="a">对方说的话，否定语（固定"不否非没弗",如确定过了直接用确定了的）</param>
/// <param name="foudingyu">是疑问句返回1，不是返回0</param>
/// <returns></returns>
bool Is_ask(const std::string& a, const std::string& foudingyu) {
	int b = a.find_first_of(foudingyu);
	if (a.find("?") != std::string::npos || a.find("？") != std::string::npos || a.find("吗") != std::string::npos) {
		return 1;
	}
	else if (a.substr(b - 3, b - 1) == a.substr(b + 3, b + 5)) {
		return 1;
	}
	else { return 0; }
}



#pragma endregion


/// <summary>
/// 遍历档案返回其id号，如果没有自动创建档案返回新id号
/// </summary>
/// <param name="userid">其QQ号</param>
/// <returns>返回其在档案里的id号</returns>
short find_id(QQID userid, GroupMessageEvent& hear){
	for (short i = 0; i < _7fang.size(); i++) {
		if (_7fang[i].qq == userid) {
			return i;
		}
	}
	xiling_dangan::addPlayer(_7fang, xiling_player(IDMAX++,userid,hear.sender.nickOrNameCard(),100,110,100), userid);

	_7fang[IDMAX-1].jinengbiao.reserve(3);
	_7fang[IDMAX - 1].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[0], &_7fang[IDMAX - 1]));
	_7fang[IDMAX - 1].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[1], &_7fang[IDMAX - 1]));
	return IDMAX-1;
}

void find_zhujuetuan(){}
void find_npc(){}
//务必上传去掉“使用”的hear
std::string wupin_send(std::string_view hear,std::vector<xiling_player>& _7fang,short userid) {

	for (auto value : _7fang[userid].buff) {
		switch(value){
		case die:
			return "虽然但是，你已经死掉了，乖乖躺好哦！（按棺材板";
		case xuanyun:
			return "你处于眩晕状态，无法使用物品。";
		default:
			break;
		}
	}
	if (_7fang[userid].beibao.empty()) {
		return "你的背包空空如也。";
	}
	int num = hear.find(" ");
	if (num == -1) {
		for (int i = 0; i < _7fang[userid].beibao.size(); i++) {
			if (_7fang[userid].beibao[i]->m_name == hear) {
				std::string shuchu = _7fang[userid].beibao[i]->send(wupin::find_wupin_1use(_7fang[userid].beibao[i]->m_bianhao), _7fang, userid);
				
				if (_7fang[userid].beibao[i]->m_naijiu == 0) {
					_7fang[userid].beibao.erase(_7fang[userid].beibao.begin() + i);
					return shuchu;
				}
				else {
					if (--_7fang[userid].beibao[i]->m_naijiu) {
						shuchu += ",当前耐久度：" + tostring<int>(_7fang[userid].beibao[i]->m_naijiu);
						return shuchu;
					}
					else {
						shuchu += ",当前耐久度为0，物品损坏消失。";
						_7fang[userid].beibao.erase(_7fang[userid].beibao.begin() + i);
						return shuchu;
					}
				}
			}
		}
		return "你的背包里没有这个物品呢~";
	}
	else {
		
		std::string_view onewupin(hear.substr(0,num));
		std::string_view useto(hear.substr(num + 1, hear.size() - num - 1));
		short useto_id = 0;
		for (num = 0; num < _7fang[userid].beibao.size(); num++) {
			if (_7fang[userid].beibao[num]->m_name == onewupin) {
				break;
			}
		}
		if (num == _7fang[userid].beibao.size()) {
			return "你的背包里没有这个物品呢~";
		}
		else {
			for (; useto_id < _7fang.size(); useto_id++) {
				if (_7fang[useto_id].name == useto) {
					break;
				}
			}
			if (useto_id == _7fang.size()) {
				return "你指定的是谁呢，请准确指定ta的名字，实在不知道ta改了什么名字，可以说“属性”+空格+ “ta的qq号码或id号”来查看对方的档案";
			}
			else {
				std::string shuchu = _7fang[userid].beibao[num]->send(wupin::find_wupin_2use(_7fang[userid].beibao[num]->m_bianhao), _7fang, userid, useto_id);
				
				if (_7fang[userid].beibao[num]->m_naijiu == 0) {
					_7fang[userid].beibao.erase(_7fang[userid].beibao.begin() + num);
					return shuchu;
				}
				else {
					if (--_7fang[userid].beibao[num]->m_naijiu) {
						shuchu += ",当前耐久度：" + tostring<int>(_7fang[userid].beibao[num]->m_naijiu);
						return shuchu;
					}
					else {
						shuchu += ",当前耐久度为0，物品损坏消失。";
						_7fang[userid].beibao.erase(_7fang[userid].beibao.begin() + num);
						return shuchu;
					}
				}
			}
		}

	}

}


//商店抽卡
std::string chouka(std::vector<xiling_player>& _7fang, short id) {
	
	srand((unsigned)time(NULL));
	//int suijishu = rand() % 100;
	//之后就按这个来：
	//if (suijishu < 15) {
	//	//出金
	//}
	
	//暂且：
	int suijishu = rand() % WUPINSHU;
#define chouka(x) case x:pushin(_7fang[id], x);return showchouka(_7fang[id], _7fang[id].beibao.back())
	switch (suijishu) {
		chouka(0);chouka(1);chouka(2);chouka(3);
		chouka(4);chouka(5);chouka(6);chouka(7);
		chouka(8);chouka(9);chouka(10);chouka(11);
		chouka(12);chouka(13);chouka(14);chouka(15);
		chouka(16);chouka(17);chouka(18);chouka(19);
		chouka(20);chouka(21);chouka(22);chouka(23);
		chouka(24);chouka(25); chouka(26);
	default:
		_log<errorlevel>(error, "商店抽卡函数 随机数越界。").m_log();
		return  "[error]商店。";
		break;
	}

}
std::string chouka_10(std::vector<xiling_player>& _7fang, short id) {
	std::string shuchu="——发光——\n"+_7fang[id].name+"抽到了：";
	srand((unsigned)time(NULL));
	for (int i = 0; i < 10; i++) {
		int suijishu = rand() % WUPINSHU;
#define chouka_10(x) case x:pushin(_7fang[id], x);showchouka_10(_7fang[id], _7fang[id].beibao.back(),shuchu);break
		switch (suijishu) {
			chouka_10(0); chouka_10(1); chouka_10(2); chouka_10(3);
			chouka_10(4); chouka_10(5); chouka_10(6); chouka_10(7);
			chouka_10(8); chouka_10(9); chouka_10(10); chouka_10(11);
			chouka_10(12); chouka_10(13); chouka_10(14); chouka_10(15);
			chouka_10(16); chouka_10(17); chouka_10(18); chouka_10(19);
			chouka_10(20); chouka_10(21); chouka_10(22); chouka_10(23);
			chouka_10(24); chouka_10(25); chouka_10(26);
		default:
			_log<errorlevel>(error, "商店抽卡函数 随机数越界。").m_log();
			return  "[error]商店。";
			break;
		}
	}
	return shuchu;
}


inline void pushin(xiling_player& one, int i) {
	one.beibao.emplace_back(std::make_unique<wupin>(shangdian[i],&one));
}
inline std::string showchouka(xiling_player& one,std::unique_ptr<wupin>& wupin) {
	return "发光——" + wupin->getstring(wupin->m_pinzhi) + "——，" + one.name + "抽到了：" + wupin->m_name;
}
inline void showchouka_10(xiling_player& one, std::unique_ptr<wupin>& wupin,std::string& shuchu) {
	shuchu +=", —"+ wupin->getstring(wupin->m_pinzhi) + "—"  + wupin->m_name;
}

//是否可以发动技能，
bool CanFadong(std::vector<xiling_player>& _7fang,short userid) {
	for (zhuangtai& value : _7fang[userid].buff) {
		if (value == die || value == bulaobusi || value==xuanyun) {
			return 0;
		}
	}
	return 1;
}


//添加自定义物品
//inline void addWupin(xiling_player& player,const wupin& one){





