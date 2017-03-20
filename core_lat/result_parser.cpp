/*
 *
 * Very silly and simple result parser
 *
 */
 
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <cstdlib>

using namespace std;

class Matrix {
public:
        int readMatrix(string fileName);
        void showMatrix(vector< vector<double> > _mat);
        vector< vector<double> > _mat;
private:
        int row;
        int col;
};
int Matrix::readMatrix(string fileName) {
        ifstream ifile(fileName.c_str(), ios::in);
        string tmp1;
        string line;
        double tmp;
        vector<double> row;
        if(ifile == NULL) {
                cerr << fileName << " not exsit." << endl;
                return -1;
        } else {
                cout << "Processing file " << fileName << endl;
        }
        _mat.clear();
        while (getline(ifile, line)) {
                istringstream istr(line);
                while (istr >> tmp1) {
                        if (tmp1 == "NA") {
                                row.push_back(0.0);
                        }
                        else {
                                // -std=c++11
                                // tmp = std::stod(tmp1);
                                tmp = atof(tmp1.c_str());
                                row.push_back(tmp);
                        }
                }
                this->col = row.size();
                _mat.push_back(row);
                row.clear();
                istr.clear();
                line.clear();
        }
        this->row = _mat.size();
        ifile.close();
        return 0;
}
void Matrix::showMatrix(vector<vector<double> > _mat) {
        for (int i = 0; i<_mat.size(); i++) {
                for (int j = 0; j<_mat[i].size(); j++) {
                        if( i == j) {
                                cout << "0\t";
                        } else {
                                //cout <<setw(7)<< _mat[i][j] << " ";
                                cout << std::fixed << std::setprecision(2) <<  _mat[i][j] << "\t";
                        }
                }
                cout << endl;
        }
}


int main(int agrc, char* argv[]) {
        Matrix m;
        vector<vector<vector<double> > > _matjz;
        string fileName = "";

        vector<vector<double> > _minjz;//min
        vector<vector<double> > _midjz;//middle
        vector<vector<double> > _maxjz;//max

        // get all input
        for (int i = 0; i < atoi(argv[1]); i++) {
                char tmpbuf[256];
                sprintf(tmpbuf, "out.%02d", i+1);
                fileName = tmpbuf;
                m.readMatrix(fileName);
                _matjz.push_back(m._mat);
        }

        // initialize
        for (int m = 0; m < _matjz[0].size(); m++) {
                for (int n = 0; n < _matjz[0][m].size(); n++) {
                        _minjz.resize(_matjz[0].size());
                        _minjz[m].resize(_matjz[0][m].size());
                        _midjz.resize(_matjz[0].size());
                        _midjz[m].resize(_matjz[0][m].size());
                        _maxjz.resize(_matjz[0].size());
                        _maxjz[m].resize(_matjz[0][m].size());
                        _minjz[m][n] = _matjz[0][m][n];
                        _midjz[m][n] = _matjz[0][m][n] / _matjz.size();
                        _maxjz[m][n] = _matjz[0][m][n];
                }
        }
        // processing
        for (int i = 1; i < _matjz.size(); i++) {
                for (int j = 0; j < _matjz[i].size(); j++) {
                        for (int k = 0; k < _matjz[i][j].size(); k++) {
                                if (_matjz[i][j][k] < _minjz[j][k]) {
                                        _minjz[j][k] = _matjz[i][j][k];
                                }
                                if (_matjz[i][j][k] > _maxjz[j][k]) {
                                        _maxjz[j][k] = _matjz[i][j][k];
                                }
                                _midjz[j][k] += _matjz[i][j][k] / _matjz.size();
                        }
                }
        }
        cout << "max:" << endl;
        m.showMatrix(_maxjz);
        cout << "min:" << endl;
        m.showMatrix(_minjz);
        cout << "avg:" << endl;
        m.showMatrix(_midjz);
        int i;
        cin >> i;
        return 0;
}

