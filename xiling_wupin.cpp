#include"xiling_wupin.h"
#include"xiling_jineng.h"
#include"xiling_npc.h"
#include"basetools.h"
#include"_log.h"
#include<memory>
#include<thread>
#include<ctime>
#include<array>

extern std::vector<jineng>alljineng;

extern std::array<xiling_npc, 4>zhujuetuan_4;
extern int num_zhujuetuan;
//extern int main_shuijinqiu_zhunbei;
extern std::vector<xiling_npc>npc;

std::string(*wupin::find_wupin_1use(int bianhao))(std::vector<xiling_player>&, short) {
	switch (bianhao) {
	case 101:
		return use_pingguo;
	case 102:
		return use_qiguaideqiu;
	case 103:
		return use_shuiqiang;

	case 201:
		return use_xiaoyaoping;
	case 202:
		return use_zhongyaoping;
	case 203:
		return use_dayaoping;
	case 204:
		return use_xiaomoliping;
	case 205:
		return use_zhongmoliping;
	case 206:
		return use_damoliping;
	case 207:
		return use_daliwan;
	case 208:
		return use_xuruoji;
	case 209:
		return use_diannaopeijian;
	case 210:
		return use_fushengdan;
	case 211:
		return use_tangsengrou;
	case 212:
		return use_hongcha;
	case 213:
		return use_xiaoduwan;
	case 214:
		return use_jieduwan;

	case 402:
		return use_meihuo;
	case 403:
		return use_naonaonao;
	case 405:
		return use_bingdun;
	case 407:
		return use_zhiliaoshu;
	case 412:
		return use_huoqiushu;
	case 414:
		return use_huanshounaqiu;
	case 415:
		return use_zhongchui;
	case 416:
		return use_shanguangdan;
	case 417:
		return use_baonue;

	case 501:
		return use_huibuliuqiushuijinqiu1;

	default:
		return use_null;
	}
	return use_null;
}
std::string(*wupin::find_wupin_2use(int bianhao))(std::vector<xiling_player>&, short, short) {
	switch (bianhao) {
	case 101:
		return use_pingguo;
	case 102:
		return use_qiguaideqiu;
	case 103:
		return use_shuiqiang;

	case 201:
		return use_xiaoyaoping;
	case 202:
		return use_zhongyaoping;
	case 203:
		return use_dayaoping;
	case 204:
		return use_xiaomoliping;
	case 205:
		return use_zhongmoliping;
	case 206:
		return use_damoliping;
	case 207:
		return use_daliwan;
	case 208:
		return use_xuruoji;
	case 209:
		return use_diannaopeijian;
	case 210:
		return use_fushengdan;
	case 211:
		return use_tangsengrou;
	case 212:
		return use_hongcha;
	case 213:
		return use_xiaoduwan;
	case 214:
		return use_jieduwan;

	case 402:
		return use_meihuo;
	case 403:
		return use_naonaonao;
	case 405:
		return use_bingdun;
	case 407:
		return use_zhiliaoshu;
	case 412:
		return use_huoqiushu;
	case 414:
		return use_huanshounaqiu;
	case 415:
		return use_zhongchui;
	case 416:
		return use_shanguangdan;
	case 417:
		return use_baonue;

	case 501:
		return use_huibuliuqiushuijinqiu1;

	default:
		return use_null;
	}
	return use_null;
}

inline bool isMianyi(std::vector<xiling_player>& _7fang, short id, zhuangtai whocanMianyi1);
inline bool isMianyi(std::vector<xiling_player>& _7fang, short id, zhuangtai whocanMianyi1, zhuangtai whocanMianyi2);
inline bool isAlreadyGet(const xiling_player& one);


std::vector<zhuangtai_chixu>chixu;

//无效果

//苹果
std::string use_pingguo(std::vector<xiling_player>& _7fang, short userid) {

	return _7fang[userid].name + "使用了苹果，清脆可口，真好吃！"; 
}
std::string use_pingguo(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return _7fang[userid].name + " 把苹果递给 " + _7fang[useto].name + " ，" + _7fang[useto].name + "一口咬住，泛着果鲜的汁液溢到嘴里，真好吃！";
}
//wupin example_pingguo("苹果", 101, white, nullptr);

//奇怪的球
std::string use_qiguaideqiu(std::vector<xiling_player>& _7fang, short userid) {

	return _7fang[userid].name + "拿出了奇怪的球，坐上去弹弹的！真好玩！（奇怪的球耐久度-1";
}
std::string use_qiguaideqiu(std::vector<xiling_player>& _7fang, short userid, short useto) {
	srand((unsigned)time(NULL));
	switch (rand() % 3) {
	case 0:return _7fang[userid].name + " 把奇怪的球扔给 " + _7fang[useto].name + " ，" + _7fang[useto].name + "一下把球拍了回来，"+ _7fang[userid].name + "抱住球急刹不住，被球带着咕隆咕隆滚跑了。（奇怪的球耐久度-1";
	case 1:return _7fang[userid].name + " 把奇怪的球扔给 " + _7fang[useto].name + " ，" + _7fang[useto].name + "一下就被弹飞了，斜躺在地，目光后知后觉地有些吃惊，皮肤沾了些许灰渍，衣衫凌乱，好可怜。（奇怪的球耐久度-1";
	case 2:
		for (std::unique_ptr<wupin>& value : _7fang[userid].beibao) {
			if (value->m_bianhao == 102) {
				value->give(_7fang[useto]);
				break;
			}
		}
		return _7fang[userid].name + " 把奇怪的球扔给 " + _7fang[useto].name + " ，" + _7fang[useto].name + "紧急情况下死死抱住了球，强大的冲量让" + _7fang[useto].name + "擦着地退了半米，这只球多少有点奇怪，" + _7fang[useto].name + "的胸口和肚子有些痒。（奇怪的球落入了" + _7fang[useto].name + "的背包，耐久度-1";
	default:
		_log<errorlevel>(error, "use_奇怪的球中，随机数越界了。").m_log();
		return "【error】奇怪的球，详见控制台";
	}
	
}
//水枪
std::string use_shuiqiang(std::vector<xiling_player>& _7fang, short userid) {

	return _7fang[userid].name + "拿出了水枪，呲呲呲！真好玩！（水枪耐久度-1";
}
std::string use_shuiqiang(std::vector<xiling_player>& _7fang, short userid, short useto) {
	srand((unsigned)time(NULL));
	switch (rand() % 3) {
	case 0:return _7fang[userid].name + " 把水枪瞄准了 " + _7fang[useto].name + " ，呲拉——，一道阳光下晶莹地闪着光的水柱喷射而出，水柱还没碰到" + _7fang[useto].name + "就在空中蒸发掉了，上海见状涨了10°。（水枪耐久度-1";
	case 1:return _7fang[userid].name + " 把水枪瞄准了 " + _7fang[useto].name + " ，呲拉——，不好瞄歪了！水柱在" + _7fang[useto].name + "旁边溅射开来，冰凉的水水浸入心田，啊啊，好凉！（水枪耐久度-1";
	case 2:
		return _7fang[userid].name + " 把水枪瞄准了 " + _7fang[useto].name + " ，呲拉——，不好瞄歪了！" + _7fang[useto].name + "这次看到了，急急往一边躲，不巧正好撞在了水柱上，清凉的水溅了满身，有些湿漉漉的，好凉爽（划掉）好可怜！（水枪耐久度-1";
	default:
		_log<errorlevel>(error, "use_水枪中，随机数越界了。").m_log();
		return "【error】水枪，详见控制台";
	}

}
											//增益

//小药瓶
std::string use_xiaoyaoping(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].health += 20;
	if (_7fang[userid].health > _7fang[userid].healthmax) { _7fang[userid].health = _7fang[userid].healthmax; }
	return _7fang[userid].name + "使用了小药瓶，恢复20点生命值，当前生命值：" + tostring(_7fang[userid].health) + "/" + tostring(_7fang[userid].healthmax);
}
std::string use_xiaoyaoping(std::vector<xiling_player>& _7fang, short userid, short useto) {
	_7fang[useto].health += 20;
	if (_7fang[useto].health > _7fang[useto].healthmax) { _7fang[useto].health = _7fang[useto].healthmax; }
	return _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了小药瓶，为其恢复20点生命值，ta当前的生命值为：" + tostring(_7fang[useto].health) + "/" + tostring(_7fang[useto].healthmax);
}
//wupin example_xiaoyaoping("小药瓶", 201, white, nullptr);

//中药瓶
std::string use_zhongyaoping(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].health += 50;
	if (_7fang[userid].health > _7fang[userid].healthmax) { _7fang[userid].health = _7fang[userid].healthmax; }
	return _7fang[userid].name + "使用了中药瓶，恢复50点生命值，当前生命值：" + tostring(_7fang[userid].health) + "/" + tostring(_7fang[userid].healthmax);

}
std::string use_zhongyaoping(std::vector<xiling_player>& _7fang, short userid, short useto) {
	_7fang[useto].health += 50;
	if (_7fang[useto].health > _7fang[useto].healthmax) { _7fang[useto].health = _7fang[useto].healthmax; }
	return _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了中药瓶，为其恢复50点生命值，ta当前的生命值为：" + tostring(_7fang[useto].health) + "/" + tostring(_7fang[useto].healthmax);

}
//大药瓶
std::string use_dayaoping(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].health += 100;
	if (_7fang[userid].health > _7fang[userid].healthmax) { _7fang[userid].health = _7fang[userid].healthmax; }

	std::thread chixujiaxue(chixu_addhealth, &_7fang, userid);//持续加血，10秒恢复20点，持续10分钟。
	chixujiaxue.detach();
	return _7fang[userid].name + "使用了大药瓶，恢复100点生命值，并在10分钟内每10秒恢复20点生命值，当前生命值：" + tostring(_7fang[userid].health) + "/" + tostring(_7fang[userid].healthmax);

}
std::string use_dayaoping(std::vector<xiling_player>& _7fang, short userid, short useto) {
	_7fang[useto].health += 100;
	if (_7fang[useto].health > _7fang[useto].healthmax) { _7fang[useto].health = _7fang[useto].healthmax; }

	std::thread chixujiaxue(chixu_addhealth, &_7fang, useto);//持续加血，10秒恢复20点，持续10分钟。
	chixujiaxue.detach();
	return _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了大药瓶，为其恢复100点生命值，并在10分钟内每10秒恢复20点生命值，ta当前的生命值为：" + tostring(_7fang[useto].health) + "/" + tostring(_7fang[useto].healthmax);

}
//小魔力瓶
std::string use_xiaomoliping(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].magic += 20;
	if (_7fang[userid].magic > _7fang[userid].magicmax) { _7fang[userid].magic = _7fang[userid].magicmax; }
	return _7fang[userid].name + "使用了小魔力瓶，恢复20点法力值，当前法力值：" + tostring(_7fang[userid].magic) + "/" + tostring(_7fang[userid].magicmax);

}
std::string use_xiaomoliping(std::vector<xiling_player>& _7fang, short userid, short useto) {
	_7fang[useto].magic += 20;
	if (_7fang[useto].magic > _7fang[useto].magicmax) { _7fang[useto].magic = _7fang[useto].magicmax; }
	return _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了小魔力瓶，为其恢复20点法力值，当前法力值：" + tostring(_7fang[useto].magic) + "/" + tostring(_7fang[useto].magicmax);

}
//中魔力瓶
std::string use_zhongmoliping(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].magic += 50;
	if (_7fang[userid].magic > _7fang[userid].magicmax) { _7fang[userid].magic = _7fang[userid].magicmax; }
	return _7fang[userid].name + "使用了中魔力瓶，恢复50点法力值，当前法力值：" + tostring(_7fang[userid].magic) + "/" + tostring(_7fang[userid].magicmax);

}
std::string use_zhongmoliping(std::vector<xiling_player>& _7fang, short userid, short useto) {
	_7fang[useto].magic += 50;
	if (_7fang[useto].magic > _7fang[useto].magicmax) { _7fang[useto].magic = _7fang[useto].magicmax; }
	return _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了中魔力瓶，为其恢复50点法力值，当前法力值：" + tostring(_7fang[useto].magic) + "/" + tostring(_7fang[useto].magicmax);

}
//大魔力瓶
std::string use_damoliping(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].magic += 100;
	if (_7fang[userid].magic > _7fang[userid].magicmax) { _7fang[userid].magic = _7fang[userid].magicmax; }

	std::thread chixuhuilan(chixu_addmagic, &_7fang, userid);
	chixuhuilan.detach();
	return _7fang[userid].name + "使用了大魔力瓶，恢复100点法力值，并在10分钟内每10秒恢复20点法力值，当前法力值：" + tostring(_7fang[userid].magic) + "/" + tostring(_7fang[userid].magicmax);

}
std::string use_damoliping(std::vector<xiling_player>& _7fang, short userid, short useto) {
	_7fang[useto].magic += 100;
	if (_7fang[useto].magic > _7fang[useto].magicmax) { _7fang[useto].magic = _7fang[useto].magicmax; }

	std::thread chixuhuilan(chixu_addmagic, &_7fang, useto);
	chixuhuilan.detach();
	return _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了大魔力瓶，恢复100点法力值，并在10分钟内每10秒恢复20点法力值，当前法力值：" + tostring(_7fang[useto].magic) + "/" + tostring(_7fang[useto].magicmax);

}
//大力丸
std::string use_daliwan(std::vector<xiling_player>& _7fang, short userid) {

	std::thread lidawuqiong(zhuangtai_10min, &_7fang, userid, daliwan);
	lidawuqiong.detach();

	return _7fang[userid].name + "使用了大力丸，瞬间肌肉爆炸，浑身充满了无穷的力量，造成伤害时额外造成对方当前生命值10%加5点伤害，持续10分钟 。 ";

}
std::string use_daliwan(std::vector<xiling_player>& _7fang, short userid, short useto) {

	std::thread lidawuqiong(zhuangtai_10min, &_7fang, useto, daliwan);
	lidawuqiong.detach();

	return _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了大力丸，" + _7fang[useto].name + "瞬间肌肉爆炸，浑身充满了无穷的力量，造成伤害时额外造成对方当前生命值10%加5点伤害，持续10分钟 。 ";

}
//虚弱剂
std::string use_xuruoji(std::vector<xiling_player>& _7fang, short userid) {

	return "此物品必须指定 对谁使用，eg:\"使用\"+\"物品名\"+ 空格 + \"指定人名字\" 。 ";
}
std::string use_xuruoji(std::vector<xiling_player>& _7fang, short userid, short useto) {

	std::thread xuruoji(zhuangtai_10min, &_7fang, useto, xuruo);
	xuruoji.detach();
	return _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了虚弱剂，" + _7fang[useto].name + "造成伤害时降低伤害量的50%，同时其的闪避强制无效，持续10分钟。";
}
//电脑配件
std::string use_diannaopeijian(std::vector<xiling_player>& _7fang, short userid) {
	if (isMianyi(_7fang, userid, bulaobusi)) {
		return _7fang[userid].name + "喝了电脑配件，酸酸甜甜的，还算不错。(头上飘过小字\"免疫\"";
	}
	zhuangtai_always(_7fang, userid, yinluan);
	return  _7fang[userid].name + "懵懵地抱着电脑配件喝了下去。眼前慢慢变得朦胧起来，身上有一点难受，似乎添加了奇怪的状态。";
}
std::string use_diannaopeijian(std::vector<xiling_player>& _7fang, short userid, short useto) {
	if (isMianyi(_7fang, userid, bulaobusi)) {
		return  _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了电脑配件，" + _7fang[userid].name + "没有怀疑喝了下去，酸酸甜甜的，还算不错。(头上飘过小字\"免疫\"";
	}
	zhuangtai_always(_7fang, useto, yinluan);
	return _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了电脑配件，" + _7fang[useto].name + "没有怀疑喝了下去。看上去有点发热，表现出微妙的忍耐神情，似乎添加了奇怪的状态。";
}
//唐僧肉
std::string use_tangsengrou(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].buff.clear();
	_7fang[userid].buff.push_back(bulaobusi);
	_7fang[userid].health = _7fang[userid].healthmax;
	_7fang[userid].magic = _7fang[userid].magicmax;
	_log<errorlevel>(green, _7fang[userid].name + " 获得不老不死状态 。 ").m_log();
	return _7fang[userid].name + "吃掉了唐僧肉，获得了长生不老、不死不灭之躯，属性与状态完全恢复，无视所有伤害，免疫大部分状态效果，但佛性慈悲，无法对他人造成伤害，直到每日清零";
}
std::string use_tangsengrou(std::vector<xiling_player>& _7fang, short userid, short useto) {
	_7fang[userid].beibao.emplace_back(std::make_unique<wupin>("唐僧肉", 211, golden, &_7fang[userid]));
	return _7fang[userid].name + "试图给" + _7fang[useto].name + "吃唐僧肉，但又觉得太珍贵舍不得，最终没能送出。";
}
//变身丸

//小毒丸
std::string use_xiaoduwan(std::vector<xiling_player>& _7fang, short userid) {
	if (isMianyi(_7fang, userid, bulaobusi)) {
		return _7fang[userid].name + "使用了小毒丸，好像没什么味道。(头上飘过小字\"免疫\"";
	}
	zhuangtai_always(_7fang, userid, zhongdu);
	return "震惊！" + _7fang[userid].name + " 居然自己吃下了小毒丸，" + _7fang[userid].name + "中毒了！每20秒降低5点生命值。";

}
std::string use_xiaoduwan(std::vector<xiling_player>& _7fang, short userid, short useto) {
	if (isMianyi(_7fang, userid, bulaobusi)) {
		return _7fang[userid].name + " 对 " + _7fang[userid].name + "使用了小毒丸，" + _7fang[userid].name + "有些困惑，这是什么，好像没什么味道。(头上飘过小字\"免疫\"";
	}
	zhuangtai_always(_7fang, useto, zhongdu);
	return _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了小毒丸，" + _7fang[useto].name + "中毒了！每20秒降低5点生命值。";

}
//解毒丸
std::string use_jieduwan(std::vector<xiling_player>& _7fang, short userid) {
	if (isMianyi(_7fang, userid, bulaobusi)) {
		return _7fang[userid].name + "解毒丸？嘎嘣嘎嘣，真好吃。(头上飘过小字\"免疫\"";
	}
	int i = _7fang[userid].buff.size() - 1;
	int du = 0;
	for (; i > -1; i--) {
		if (_7fang[userid].buff[i] == zhongdu) {
			_7fang[userid].buff.erase(_7fang[userid].buff.begin() + i);
			du++;
		}else if (_7fang[userid].buff[i] == hunshui) {
			_7fang[userid].buff.erase(_7fang[userid].buff.begin() + i);
			du++;
		}else if (_7fang[userid].buff[i] == yinluan) {
			_7fang[userid].buff.erase(_7fang[userid].buff.begin() + i);
			du++;
		}
		else if (_7fang[userid].buff[i] == meihuo_acceptance) {
			_7fang[userid].buff.erase(_7fang[userid].buff.begin() + i);
			du++;
		}
	}
	zhuangtai_erase(_7fang, userid, zhongdu);
	if (du) {
		return _7fang[userid].name + "使用了解毒丸，浑身舒畅，共解除了 " + tostring(du) + "种毒素。";
	}
	else {
		zhuangtai_always(_7fang, userid, zhongdu);
		return _7fang[userid].name + "使用了解毒丸，但其身上没有毒素，是药三分毒，" + _7fang[userid].name + "中毒了！每20秒降低5点生命值。";
	}


}
std::string use_jieduwan(std::vector<xiling_player>& _7fang, short userid, short useto) {
	if (isMianyi(_7fang, userid, bulaobusi)) {
		return _7fang[userid].name + "对" + _7fang[userid].name + "使用了解毒丸，" + _7fang[userid].name + "尝了尝，有点苦，不怎么好吃。(头上飘过小字\"免疫\"";
	}
	int i = _7fang[useto].buff.size() - 1;
	int du = 0;
	for (; i > -1; i--) {
		if (_7fang[useto].buff[i] == zhongdu) {
			_7fang[useto].buff.erase(_7fang[useto].buff.begin() + i);
			du++;
		}
		else if (_7fang[userid].buff[i] == hunshui) {
			_7fang[userid].buff.erase(_7fang[userid].buff.begin() + i);
			du++;
		}
		else if (_7fang[userid].buff[i] == yinluan) {
			_7fang[userid].buff.erase(_7fang[userid].buff.begin() + i);
			du++;
		}
		else if (_7fang[userid].buff[i] == meihuo_acceptance) {
			_7fang[userid].buff.erase(_7fang[userid].buff.begin() + i);
			du++;
		}
	}
	zhuangtai_erase(_7fang, useto, zhongdu);
	if (du) {
		return _7fang[userid].name + "对" + _7fang[useto].name + "使用了解毒丸，共解除了" + tostring(du) + "种毒素。";
	}
	else {
		zhuangtai_always(_7fang, userid, zhongdu);
		return _7fang[userid].name + "对" + _7fang[useto].name + "使用了解毒丸，但其身上没有毒素，是药三分毒，" + _7fang[useto].name + "中毒了！每分钟降低5点生命值。";
	}

}
//红茶
std::string use_hongcha(std::vector<xiling_player>& _7fang, short userid) {
	if (isMianyi(_7fang, userid, bulaobusi)) {
		return _7fang[userid].name + "使用了红茶, 蛮好喝的。(头上飘过一个小号的\"免疫\"";
	}
	zhuangtai_always(_7fang, userid, hunshui);
	return _7fang[userid].name + "使用了红茶，蛮好喝的....Zzzzzzz";
}
std::string use_hongcha(std::vector<xiling_player>& _7fang, short userid, short useto) {
	if (isMianyi(_7fang, useto, bulaobusi)) {
		return _7fang[userid].name + +" 对 " + _7fang[useto].name + "使用了红茶,"+ _7fang[userid].name + "一口喝光了，挺好喝的。（头上飘过小字\"免疫\"";
	}
	zhuangtai_always(_7fang, useto, hunshui);
	_7fang[useto].healthmax += 20;
	_7fang[useto].health += 50;
	if (_7fang[useto].health > _7fang[useto].healthmax)_7fang[useto].health = _7fang[useto].healthmax;
	return _7fang[userid].name + +" 对 " + _7fang[useto].name + "使用了红茶，其提升20点生命值上限，恢复50点生命值，真好喝！" + _7fang[useto].name + "对" + _7fang[userid].name + "好感度+20，但感到意识有点模糊……." + _7fang[useto].name + "睡着了！怎么办，在线等！";

}
//复生丹
std::string use_fushengdan(std::vector<xiling_player>& _7fang, short userid) {
	return _7fang[userid].name + "使用了复生丹，嘎嘣嘎嘣！真好吃！";

}
std::string use_fushengdan(std::vector<xiling_player>& _7fang, short userid, short useto) {
	for (zhuangtai& value : _7fang[useto].buff) {
		if (value == die) {

			zhuangtai_erase(_7fang, useto, die);
			_7fang[useto].health = _7fang[useto].healthmax;
			return  _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了复生丹，" + _7fang[useto].name + "满血复活了！";
		}
	}
	
		return _7fang[userid].name + " 对 " + _7fang[useto].name + "使用了复生丹，" + _7fang[useto].name + "一把扔到了嘴里，嘎嘣嘎嘣！真好吃！";
	
}



//好感度

//棒棒糖
std::string use_bangbangtang(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return "";
}
//奶茶泡方便面
std::string use_naichapaofangbianmian(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return "";
}
//电线杆
std::string use_dianxiangan(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return "";
}
//包子皮
std::string use_baozipi(std::vector<xiling_player>& _7fang, short userid, std::string& xian) {
	//使用人，包子馅

	return "";
}
std::string use_baozipi(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return "";
}
//爱心小蛋糕
std::string use_aixinxiaodangao(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "";

}
//爱心小饼干
std::string use_aixinxiaobinggan(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "";

}
//猫耳耳机
std::string use_maoererji(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "";

}
//肥宅快乐水
std::string use_feizhaikuaileshui(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return "";
}
//雪糕
std::string use_xuegao(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return "";
}
//冰激凌
std::string use_bingjiling(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return "";
}
//逗猫棒
std::string use_doumaobang(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "";

}
//冰夜糖果
std::string use_bingyetangguo(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return "";
}
//天狼星
std::string use_tianlangxing(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return "";
}
//女仆装
std::string use_nvpuzhuang(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "";

}
//《c++从入门到入土》
std::string use_shu1(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return "";
}
//《量子力学》
std::string use_shu2(std::vector<xiling_player>& _7fang, short userid, short useto) {

	return "";
}
//《魔法基础导示》
std::string use_shu3(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "";

}
//《线性可塑型魔法》
std::string use_shu4(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "";

}
//《魔力引导基础》
std::string use_shu5(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "";

}
//《月光城女仆守则》
std::string use_shu6(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "";

}






//技能书

//石化
std::string use_shihua(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "";
}
//魅惑
std::string use_meihuo(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[9],&_7fang[userid]));
	return "成功使用魅惑技能书，获得魅惑技能。";
}
std::string use_meihuo(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "非封印类技能卷轴无法对他人使用。";
}
//挠挠挠
std::string use_naonaonao(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[8],&_7fang[userid]));
	return "成功使用挠挠挠技能书，获得挠挠挠技能。";
}
std::string use_naonaonao(std::vector<xiling_player>& _7fang, short userid, short useto ) {
	return "非封印类技能卷轴无法对他人使用。";
}
//矮子望天
std::string use_aiziwangtian(std::vector<xiling_player>& _7fang, short userid) {
	return "";
}
//冰盾
std::string use_bingdun(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[6],&_7fang[userid]));
	return "成功使用冰盾技能书，获得冰盾技能。";
}
std::string use_bingdun(std::vector<xiling_player>& _7fang, short userid,short useto) {
	return "非封印类技能卷轴无法对他人使用。";
}
//召唤术
std::string use_zhaohuanshu(std::vector<xiling_player>& _7fang, short userid) {
	return "";
}
//治疗术
std::string use_zhiliaoshu(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[5], &_7fang[userid]));
	return "成功使用治疗术技能书，获得治疗术技能。";
}
std::string use_zhiliaoshu(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "非封印类技能卷轴无法对他人使用。";
}
//替身术
std::string use_tishenshu(std::vector<xiling_player>& _7fang, short userid) {

	return "";
}
//左右横跳
std::string use_zuoyouhengtiao(std::vector<xiling_player>& _7fang, short userid) {

	return "";
}
//风神的加护
std::string use_fengshendejiahu(std::vector<xiling_player>& _7fang, short userid) {

	return "";
}
//现身吧！奥兹
std::string use_xianshenbaaozi(std::vector<xiling_player>& _7fang, short userid) {

	return "";
}
//火球术
std::string use_huoqiushu(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[7], &_7fang[userid]));
	return "成功使用火球术技能书，获得火球术技能。";
}
std::string use_huoqiushu(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "非封印类技能卷轴无法对他人使用。";

}
//冰封千里
std::string use_bingfengqianli(std::vector<xiling_player>& _7fang, short userid) {

	return "";
}
//换手拿球
std::string use_huanshounaqiu(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[4], &_7fang[userid]));
	return "成功使用换手拿球技能书，获得换手拿球技能。";
}
std::string use_huanshounaqiu(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "非封印类技能卷轴不能对他人使用。";
}
//重锤
std::string use_zhongchui(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[10], &_7fang[userid]));
	return "成功使用重锤技能书，获得重锤技能。";
}
std::string use_zhongchui(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "非封印类技能卷轴不能对他人使用。";
}
//闪光弹
std::string use_shanguangdan(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[11], &_7fang[userid]));
	return "成功使用闪光弹技能书，获得闪光弹技能。";
}
std::string use_shanguangdan(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "非封印类技能卷轴不能对他人使用。";
}
//暴虐
std::string use_baonue(std::vector<xiling_player>& _7fang, short userid) {
	_7fang[userid].jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[12], &_7fang[userid]));
	return "成功使用暴虐技能书，获得暴虐技能。";
}
std::string use_baonue(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "非封印类技能卷轴不能对他人使用。";
}


//水晶球

//灰不溜秋水晶球
std::string use_huibuliuqiushuijinqiu1(std::vector<xiling_player>& _7fang, short userid) {
	
	npc.reserve(7);
	npc.emplace_back( "【蘑菇剑士】","芙拉里·绯尔温斯",
		"一个自称蘑菇的剑士。", 
		2000, 500,  140, 50, 100, 100, 10000, &_7fang[0]);
	npc.emplace_back("【商人】","艾德·皮斯科",
		"边缘贵族之子，拥有靠近骨山的边缘领地，但祖上祖下仍然有一个富翁梦并坚持较危险的经商。",
		600, 100, 40, 120, 50, 50, 1000000);
	npc.emplace_back("【马夫】","狗蛋",
		"一个快乐的马夫。",
		300, 10, 30, 30, 20, 20,100 );
	npc.emplace_back( "【马夫】","傻蛋",
		"一个超快乐的马夫。", 
		300, 10, 30, 30, 20, 20, 100);
	npc.emplace_back("【？？？】","库谷·？？？",
		"",
		5000,20000,20, 200, 70,70, 10000);
	npc.emplace_back("【二王子】","古路拉·绯尔温斯",
		"绯尔温斯帝国的二王子。",
		1000,3000,70, 120, 80,80, 10000);


	//主角团初始化：
	zhujuetuan_4[0] = xiling_npc("", "茎秩", "弓箭手，气质卓雅，由茎秩交涉npc或许有奇效。", 300,160, 15, 20, 20, 30, 50);
	zhujuetuan_4[1] = xiling_npc("", "好名字都被狗取了", "剑士，自称是网界来的人，信息风向敏锐。", 400, 80, 20, 10, 30, 20, 50);
	zhujuetuan_4[2] = xiling_npc("", "爸爸", "法师，很菜很嚣张，女王大人来了也得喊他一声爸爸，这个误会如果能利用的话或许有奇效。", 200, 1000, 5, 70, 15, 15, 50);
	zhujuetuan_4[3] = xiling_npc("", "那个人", "骑士，比伏地魔还神秘的人，这个误会如果能利用的话或许有奇效。", 1000, 120,15, 5, 50, 50, 50);


	//main_shuijinqiu_zhunbei = 1;

	return "灰不溜秋的水晶球发出了灰不溜秋的光芒，将此地空间投射，" + _7fang[userid].name +
		"被投射到灰不溜秋的秘境。\n此秘境为4人秘境，预计5~10分钟。开启后所有人与外界失去联系，任意未加入者可默念“加入”后选择主角或开启后默念“附身”选择npc，主角团满1人后可默念“开启”开启。";
}
std::string use_huibuliuqiushuijinqiu1(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "水晶球物品无法对他人使用。";
}


//红水晶球
std::string use_hongshuijinqiu(std::vector<xiling_player>& _7fang, short userid) {

	return "";
}
//紫水晶球
std::string use_zishuijinqiu(std::vector<xiling_player>& _7fang, short userid) {

	return "";
}
//蓝水晶球
std::string use_lanshuijinqiu(std::vector<xiling_player>& _7fang, short userid) {
	return "";

}
//粉水晶球
std::string use_fenshuijinqiu(std::vector<xiling_player>& _7fang, short userid) {
	return "";

}
//绿水晶球 
std::string use_lvshuijinqiu(std::vector<xiling_player>& _7fang, short userid) {

	return "";
}
//橘水晶球
std::string use_jushuijinqiu(std::vector<xiling_player>& _7fang, short userid) {
	return "";

}
//白水晶球
std::string use_baishuijinqiu(std::vector<xiling_player>& _7fang, short userid) {

	return "";
}
//黑水晶球
std::string use_heishuijinqiu(std::vector<xiling_player>& _7fang, short userid) {

	return "";
}



//查无此物品
std::string use_null(std::vector<xiling_player>& _7fang, short userid) {
	_log<errorlevel>(error, " 查表没有找到物品 " + defaultlog()).m_log();
	return "[鬼来了也出不来这个bug] 不存在这个物品";
}
std::string use_null(std::vector<xiling_player>& _7fang, short userid, short useto) {
	_log<errorlevel>(error, " 查表没有找到物品 " + defaultlog()).m_log();
	return "[鬼来了也出不来这个bug] 不存在这个物品";
}







//持续加血：
void chixu_addhealth(std::vector<xiling_player>* _7fang, short id) {
	using namespace std::literals::chrono_literals;
	int i = 60;
	while (i--) {
		(* _7fang)[id].health += 20;
		if ((*_7fang)[id].health > (*_7fang)[id].healthmax)(*_7fang)[id].health = (*_7fang)[id].healthmax;

		_log<errorlevel>(green, (*_7fang)[id].name + " 持续恢复生命 + 20 。").m_log();
		std::this_thread::sleep_for(10s);

	}
	_log<errorlevel>(green, (*_7fang)[id].name + " 的 持续恢复状态 结束 。 ").m_log();
}

//持续回蓝：
void chixu_addmagic(std::vector<xiling_player>* _7fang, short id) {
	using namespace std::literals::chrono_literals;
	int i = 60;
	while (i--) {
		(*_7fang)[id].magic += 20;
		if ((*_7fang)[id].magic > (*_7fang)[id].magicmax)(*_7fang)[id].magic = (*_7fang)[id].magicmax;

		_log<errorlevel>(green, (*_7fang)[id].name + " 持续恢复法力 + 20 。").m_log();
		std::this_thread::sleep_for(10s);

	}
	_log<errorlevel>(green, (*_7fang)[id].name + " 的 持续恢复状态 结束 。 ").m_log();
}

//中毒
void chixu_zhongdu(std::vector<xiling_player>* _7fang, short id) {
	using namespace std::literals::chrono_literals;

	chixu.push_back(zhuangtai_chixu(1,id));
	int a = chixu.size() - 1;
	while (chixu[a].m_isExist) {

		std::this_thread::sleep_for(20s);
		(* _7fang)[id].health -= 5;
		_log<errorlevel>(green, "毒素发作扣除5点生命值。").m_log();
		if ((*_7fang)[id].health < 1) {
			zhuangtai_always(*_7fang, id, die);
			chixu[a].m_isExist = 0;
		}

	}

	chixu.erase(chixu.begin() + a);
}

//持续状态效果,持续10分钟
void zhuangtai_10min(std::vector<xiling_player>* _7fang, short id, zhuangtai buff) {
	using namespace std::literals::chrono_literals;

	(*_7fang)[id].buff.push_back(buff);
	int i = (*_7fang)[id].buff.size() - 1;
	_log<errorlevel>(green, (*_7fang)[id].name + " 获得" + tostring((int)buff) + "号状态，将持续10分钟 。 ").m_log();


	std::this_thread::sleep_for(10min);


	(*_7fang)[id].buff.erase((*_7fang)[id].buff.begin() + i);
	_log<errorlevel>(green, (*_7fang)[id].name + " 的 " + tostring((int)buff) + "号状态 持续时间结束 。 ").m_log();


}






//添加状态效果（永久，除非再解除-
void zhuangtai_always(std::vector<xiling_player>& _7fang, short id, zhuangtai onebuff) {

	if (onebuff == zhongdu) {
		std::thread zhongdu(chixu_zhongdu, &_7fang, id);
		zhongdu.detach();
	}
	_7fang[id].buff.push_back(onebuff);
	int i = _7fang[id].buff.size() - 1;
	_log<errorlevel>(green, _7fang[id].name + " 获得" + tostring((int)onebuff) + "号状态 。 ").m_log();

}

//解除状态效果
void zhuangtai_erase(std::vector<xiling_player>& _7fang, short id, zhuangtai onebuff) {

	if (onebuff == zhongdu) {
		for (zhuangtai_chixu& a : chixu) {
			if (a.m_id == id) {
				a.m_isExist = 0;
			}
		}
	}
	int i = _7fang[id].buff.size() - 1;
	for (; i > -1; i--) {
		if (_7fang[id].buff[i] == onebuff)_7fang[id].buff.erase(_7fang[id].buff.begin() + i);
	}

	_log<errorlevel>(green, _7fang[id].name + " 的所有 " + tostring((int)onebuff) + "号状态 被解除。 ").m_log();

}







//以下两个免疫与否，统一放到最底层函数里

/// <summary>
/// 是否可以免疫此效果
/// </summary>
/// <param name="_7fang">_7fang</param>
/// <param name="id">id号</param>
/// <param name="whocanMianyi1">什么状态可以免疫</param>
/// <returns>是否可以免疫</returns>
inline bool isMianyi(std::vector<xiling_player>& _7fang, short id, zhuangtai whocanMianyi1) {
	if (_7fang[id].buff.empty()) {
		return 0;
	}
	for (zhuangtai& value : _7fang[id].buff) {
		if (value == whocanMianyi1) {
			return 1;
		}
	}
	return 0;
}

/// <summary>
/// 是否可以免疫此效果
/// </summary>
/// <param name="_7fang">_7fang</param>
/// <param name="id">id号</param>
/// <param name="whocanMianyi1">什么状态可以免疫</param>
/// <param name="whocanMianyi2">什么状态可以免疫</param>
/// <returns>是否可以免疫</returns>
inline bool isMianyi(std::vector<xiling_player>& _7fang, short id, zhuangtai whocanMianyi1,zhuangtai whocanMianyi2) {
	if (_7fang[id].buff.empty()) {
		return 0;
	}
	for (zhuangtai& value : _7fang[id].buff) {
		if (value == whocanMianyi1||value==whocanMianyi2) {
			return 1;
		}
	}
	return 0;
}


//是否存在重复技能,暂时没使用，反正多的无效！
inline bool isAlreadyGet(const xiling_player& one) {
	for (int i = 0; i < one.jinengbiao.size();i++) {
		for (int j = 0; j < one.jinengbiao.size(); j++) {
			if (one.jinengbiao[i]==one.jinengbiao[j]&&i!=j) {
				return 1;
			}
		}
	}
	return 0;
}