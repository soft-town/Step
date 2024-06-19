// Step.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <iostream>
#include <string>
#include <list>
#include <vector>  
#include <algorithm>
#include <direct.h>

using namespace std;
struct _pair
{
	float t;
	float v;
	_pair() {
		t = v = 0;
	}
	_pair(float t_, float v_) {
		t = t_, v = v_;
	}
};

// fc カットオフ
// 　RC = 1 / (2*π*fc)
// 　a = Δt/(RC + Δt)
// 　y[i] = a*x[i] +(1-a)*y[i-1]
//
void lowpass(std::vector<_pair>& wavei, std::vector<_pair>& waveo, float fc)
{
	double RC = 1 / (2 * M_PI * fc);
	double dt = wavei[1].t - wavei[0].t;
	double a = dt / (RC + dt);

	waveo.resize(wavei.size());
	waveo[0] = wavei[0];
	for (int i = 0; i + 1 < wavei.size(); i++) {
		waveo[i + 1].v = a * wavei[i + 1].v + (1 - a) * wavei[i].v;
		waveo[i + 1].t = wavei[i + 1].t;
	}
	printf("Lowpass fc=%10.3e, a=%10.3e\n", fc, a);
}


//
bool pair_compare_x(const _pair& p1, const _pair& p2)
{
	return p1.t < p2.t;
}

//
bool read_csv(char *fname, vector<_pair> &g)
{
	FILE* fp = fopen(fname, "r");
	if (fp == NULL)
		return false;

	char buff[256];
	fgets(buff, 256, fp); // ヘッダ

	float t, v;
	while (fgets(buff, 256, fp)) {
		if (buff[0] == '\n') 
			continue;
		2 == sscanf(buff, "%f %f\n", &t, &v);
		g.push_back(_pair(t, v));
	}
	fclose(fp);

	sort(g.begin(), g.end(), pair_compare_x);
	return true;
}

//
bool write_csv(char* fname, vector<_pair>& g)
{
	FILE* fp = fopen(fname, "w");
	if (fp == NULL)
		return false;

	char buff[256];
	fprintf(fp, "Time[s]	Normalized voltage[p.u.]\n");

	for (int i=0; i<g.size(); i++) {
		fprintf(fp, "%10.7e %10.7e\n", g[i].t, g[i].v);
	}
	fclose(fp);

	return true;
}

// 関数(g)が単調増加を開始する時刻O1を求める
//
float detOrg(vector<_pair>& g)
{
	int i = 0;
#define CASE 3
#if CASE==1
	// eps=0.01を超えた点をO1
	while (g[i].v <= 0.01) {
		i++;
	}
#elif CASE==2
	// データ開始後50000サンプルの振幅
	float s = 0;
	int n = 50000;
	for (i = 0; i < n; i++)
		s += fabs(g[i].v);
	s /= n;
	i = 0;
	while (g[i].v <= s * 100) {
		i++;
	}
#else	// 電中研が設定したt<0を利用
	while (g[i].t <= 0) {
		i++;
	}
#endif
	printf("O1=%d, %10.7e\n", i, g[i].t);
	return g[i].t;	
}


// T(t)を求める、関数(1-g)をステップ応答原点O1から全区間積分
 
void integralT(float O1, vector<_pair>& g, vector<_pair>& T)
{
	int i;
	float s = 0;
	for (int i=0; i<g.size(); i++) {
		if (g[i].t < O1) {
			T.push_back(_pair(g[i].t, 0));
			continue;
		}
		s += (1 - g[i].v) * (g[i].t - g[i - 1].t);
		T.push_back(_pair(g[i].t, s));
	}
}

// T (t)の最大値

int calc_Ta(vector<_pair>&T, _pair& Ta)
{
	int i;
	float mv = FLT_MIN;
	for (i = 0; i < T.size(); i++) {
		if (mv < T[i].v) {
			mv = T[i].v;
			Ta = T[i];
		}
	}
	return i;
}

// fabs(TN-G(t))<0.02tが成り立つ最小のt

float search_ts(vector<_pair>& T, float TN)
{
	int i;
	for (i = T.size()-1; 0 <= i; i--) {
		if(0.02 * T[i].t < fabs(T[i].v - TN)) break;
	}
	if (0<=i) {
		return T[i].t;
	}
	return 0;
}

// Main
int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Usage: step input-file {NS | TAMA]\n");
	}

	// 入力ファイル読み込み
	char buff[256];
	sprintf(buff, "%s\\%s", argv[2], argv[1]);
	vector<_pair> g;
	if (!read_csv(buff, g))
		return -1;

	// 時間ソートしたデータを保存
	sprintf(buff, "%s\\S-%s", argv[2], argv[1]);
	write_csv(buff, g);

	// Lowpass
	vector<_pair> go;
	float fc = 1 / (g[1].t-g[0].t);  // 1e-8
	lowpass(g, go, fc);
	sprintf(buff, "%s\\Lowpass.txt", argv[2]);
	write_csv(buff, go);

	// TN実験応答時間 T(2*tmax)
	float tmin = 0.42e-6*2;
	float tmax = 3.12e-6/2;
	float TN = 0;   // 実験応答時間
	_pair Ta;       // 部分応答時間
	float ts = 0;   // 整定時間

	// 原点O1
	float O1 = detOrg(go);
	// O1 = -2e-8; //!ST

	// T(t)の計算
	vector<_pair> T;
	integralT(O1, g, T);
	sprintf(buff, "%s\\T(t).txt", argv[2]);
	write_csv(buff, T);

	// TNの計算
	int i;
	for (i = 0; T[i].t < O1; i++);
	for (i++; T[i].t < 2 * tmax; i++);
	TN = T[i].v;
	printf("TN=%10.3e\n", TN);

	// Ta 部分応答時間 max(T(t)) t<2*tmax, （通常Ta=T(t1)ここでt1はg(t)=1となる最小のt？）
	calc_Ta(T, Ta);
	printf("Ta=%10.3e\n", Ta.v);

	// ts 整定時間
	ts = search_ts(T, TN);
	printf("ts=%10.3e\n", ts);

	return 0;
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//   1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します
