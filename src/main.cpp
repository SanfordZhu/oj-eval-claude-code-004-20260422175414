#include <bits/stdc++.h>
using namespace std;

struct Account {
    string user_id, password, username;
    int privilege = 1;
};

struct Book {
    string isbn;
    string name;
    string author;
    vector<string> keywords; // stored in input order
    double price = 0.0;
    long long stock = 0;
};

struct FinanceItem {
    long long income_cents = 0;
    long long expenditure_cents = 0;
};

// Simple persistent storage to files under ./data
// We must keep file count under 20; we use 4 files: accounts, books, finance, opslog
static const string ACC_FILE = "accounts.dat";
static const string BOOK_FILE = "books.dat";
static const string FIN_FILE = "finance.dat";
static const string LOG_FILE = "ops.log";

// Utility
static inline string trim(const string &s){
    size_t i=0,j=s.size();
    while(i<j && isspace((unsigned char)s[i])) ++i;
    while(j>i && isspace((unsigned char)s[j-1])) --j;
    return s.substr(i,j-i);
}

static vector<string> split_spaces(const string &line){
    vector<string> parts; string cur; bool in_quotes=false; char quote='\0';
    for(size_t i=0;i<line.size();++i){
        char c=line[i];
        if(in_quotes){
            if(c==quote){ in_quotes=false; }
            else cur.push_back(c);
        }else{
            if(c=='"'){ in_quotes=true; quote='"'; }
            else if(isspace((unsigned char)c)){
                if(!cur.empty()){ parts.push_back(cur); cur.clear(); }
            }else{
                cur.push_back(c);
            }
        }
    }
    if(!cur.empty()) parts.push_back(cur);
    return parts;
}

static bool is_ascii_visible_except_quotes(const string &s){
    for(char c: s){
        if(c=='"' || c=='\n' || c=='\r' || c=='\t') return false;
        if((unsigned char)c<32 || (unsigned char)c>126) return false;
    }
    return true;
}
static bool is_ascii_visible(const string &s){
    for(char c: s){
        if(c=='\n'||c=='\r'||c=='\t') return false;
        if((unsigned char)c<32 || (unsigned char)c>126) return false;
    }
    return true;
}
static bool is_id_char(char c){ return isalnum((unsigned char)c) || c=='_'; }
static bool is_legal_id(const string &s){ if(s.size()>30||s.empty()) return false; for(char c: s) if(!is_id_char(c)) return false; return true; }
static bool is_legal_username(const string &s){ return !s.empty() && s.size()<=30 && is_ascii_visible(s); }
static bool is_legal_priv(const string &s){ if(s.size()!=1 || !isdigit((unsigned char)s[0])) return false; int v=s[0]-'0'; return v==1||v==3||v==7; }
static bool is_legal_isbn(const string &s){ return !s.empty() && s.size()<=20 && is_ascii_visible(s); }
static bool is_legal_name_author(const string &s){ return !s.empty() && s.size()<=60 && is_ascii_visible_except_quotes(s); }
static bool is_legal_keyword_one(const string &s){ return !s.empty() && s.size()<=60 && is_ascii_visible_except_quotes(s); }

static bool parse_positive_int(const string &s, long long &v){
    if(s.empty()) return false; v=0; for(char c: s){ if(!isdigit((unsigned char)c)) return false; v = v*10 + (c-'0'); if(v>2147483647LL) return false; }
    return v>0;
}
static bool parse_price(const string &s, long long &cents){ // two decimals
    if(s.empty()) return false; int dot=-1; for(int i=0;i<(int)s.size();++i){ if(s[i]=='.'){ if(dot!=-1) return false; dot=i; } else if(!isdigit((unsigned char)s[i])) return false; }
    string a,b; if(dot==-1){ a=s; b="00"; }
    else { a=s.substr(0,dot); b=s.substr(dot+1); }
    if(b.size()>2) return false; while(b.size()<2) b.push_back('0');
    if(a.size()>13) return false; // input length limit
    // disallow leading zeros? spec not strict; allow
    long long A=0,B=0; for(char c: a){ A = A*10 + (c-'0'); }
    for(char c: b){ B = B*10 + (c-'0'); }
    cents = A*100 + B; return true;
}
static string format_price(long long cents){
    long long A=cents/100; long long B=cents%100;
    string s = to_string(A) + "." + (B<10?"0":"") + to_string(B);
    return s;
}

// Persistent maps
static unordered_map<string, Account> accounts;
static unordered_map<string, Book> books;
static vector<FinanceItem> finances;
static vector<string> opslog;

static void load_all(){
    // accounts
    ifstream fa(ACC_FILE); if(fa){ string line; while(getline(fa,line)){ if(line.empty()) continue; stringstream ss(line); Account a; ss>>a.user_id>>a.password; ss>>a.privilege; string rest; getline(ss, rest); a.username = trim(rest); accounts[a.user_id]=a; } }
    if(accounts.empty()){ Account root; root.user_id="root"; root.password="sjtu"; root.username="root"; root.privilege=7; accounts[root.user_id]=root; }

    ifstream fb(BOOK_FILE); if(fb){ string line; while(getline(fb,line)){ if(line.empty()) continue; // isbn\tname\tauthor\tkw|kw2\tprice_cents\tstock
            string isbn,name,author,kw; long long price=0; long long stock=0; stringstream ss(line); getline(ss,isbn,'\t'); getline(ss,name,'\t'); getline(ss,author,'\t'); getline(ss,kw,'\t'); ss>>price; ss>>stock; Book b; b.isbn=isbn; b.name=name; b.author=author; b.price=price/100.0; b.stock=stock; // keep keywords order
            b.keywords.clear(); string cur; stringstream ks(kw); while(getline(ks,cur,'|')) if(!cur.empty()) b.keywords.push_back(cur); books[isbn]=b; }
    }
    ifstream ff(FIN_FILE); if(ff){ string line; while(getline(ff,line)){ if(line.empty()) continue; stringstream ss(line); FinanceItem it; ss>>it.income_cents>>it.expenditure_cents; finances.push_back(it); } }
    ifstream fl(LOG_FILE); if(fl){ string line; while(getline(fl,line)){ opslog.push_back(line); } }
}

static void save_accounts(){ ofstream fa(ACC_FILE, ios::trunc); for(auto &p: accounts){ const Account &a=p.second; fa<<a.user_id<<" "<<a.password<<" "<<a.privilege<<" "<<a.username<<"\n"; } }
static void save_books(){ ofstream fb(BOOK_FILE, ios::trunc); for(auto &p: books){ const Book &b=p.second; string kw; for(size_t i=0;i<b.keywords.size();++i){ if(i) kw.push_back('|'); kw+=b.keywords[i]; }
    long long price_cents = llround(b.price*100.0);
    fb<<b.isbn<<"\t"<<b.name<<"\t"<<b.author<<"\t"<<kw<<"\t"<<price_cents<<"\t"<<b.stock<<"\n"; } }
static void save_fin(){ ofstream ff(FIN_FILE, ios::trunc); for(auto &it: finances){ ff<<it.income_cents<<" "<<it.expenditure_cents<<"\n"; } }
static void save_log(){ ofstream fl(LOG_FILE, ios::trunc); for(auto &l: opslog){ fl<<l<<"\n"; } }

struct SessionEntry{ string user_id; int privilege; string selected_isbn; };
static vector<SessionEntry> session;

static int current_priv(){ return session.empty()?0:session.back().privilege; }
static string current_user(){ return session.empty()?string():session.back().user_id; }
static string current_selected(){ return session.empty()?string():session.back().selected_isbn; }

static void push_log(const string &line){ opslog.push_back(line); }

int main(){
    ios::sync_with_stdio(false); cin.tie(nullptr);
    load_all();
    string raw;
    while(true){
        if(!std::getline(cin, raw)) break;
        string line = trim(raw);
        if(line.empty()){ continue; }
        auto parts = split_spaces(line);
        if(parts.empty()) { continue; }
        string cmd = parts[0];
        auto invalid = [&](){ cout<<"Invalid\n"; };
        if(cmd=="quit"||cmd=="exit"){ break; }
        else if(cmd=="su"){
            if(parts.size()<2 || parts.size()>3){ invalid(); continue; }
            string uid=parts[1]; string pwd = parts.size()==3?parts[2]:string();
            if(!is_legal_id(uid)){ invalid(); continue; }
            auto it = accounts.find(uid); if(it==accounts.end()){ invalid(); continue; }
            int curp = current_priv();
            if(parts.size()==2){ if(curp<=it->second.privilege){ invalid(); continue; } }
            else { if(pwd!=it->second.password){ invalid(); continue; } }
            session.push_back({uid, it->second.privilege, string()});
            push_log("su "+uid);
        }
        else if(cmd=="logout"){
            if(current_priv()<1 || session.empty()){ invalid(); continue; }
            session.pop_back();
            push_log("logout");
        }
        else if(cmd=="register"){
            if(parts.size()!=4){ invalid(); continue; }
            string uid=parts[1], pwd=parts[2], uname=parts[3];
            if(!is_legal_id(uid)||!is_legal_id(pwd)||!is_legal_username(uname)){ invalid(); continue; }
            if(accounts.count(uid)){ invalid(); continue; }
            Account a; a.user_id=uid; a.password=pwd; a.username=uname; a.privilege=1; accounts[uid]=a; save_accounts(); push_log("register "+uid);
        }
        else if(cmd=="passwd"){
            if(parts.size()!=3 && parts.size()!=4){ invalid(); continue; }
            string uid=parts[1]; if(!is_legal_id(uid)){ invalid(); continue; }
            auto it=accounts.find(uid); if(it==accounts.end()){ invalid(); continue; }
            if(parts.size()==3){ // omit current if current is 7
                if(current_priv()!=7){ invalid(); continue; }
                string newp=parts[2]; if(!is_legal_id(newp)){ invalid(); continue; }
                it->second.password=newp; save_accounts(); push_log("passwd "+uid);
            }else{
                string curp=parts[2], newp=parts[3]; if(!is_legal_id(curp)||!is_legal_id(newp)){ invalid(); continue; }
                if(curp!=it->second.password){ invalid(); continue; }
                it->second.password=newp; save_accounts(); push_log("passwd "+uid);
            }
        }
        else if(cmd=="useradd"){
            if(parts.size()!=5){ invalid(); continue; }
            if(current_priv()<3){ invalid(); continue; }
            string uid=parts[1], pwd=parts[2], privs=parts[3], uname=parts[4];
            if(!is_legal_id(uid)||!is_legal_id(pwd)||!is_legal_priv(privs)||!is_legal_username(uname)){ invalid(); continue; }
            int pv = privs[0]-'0'; if(pv>=current_priv()){ invalid(); continue; }
            if(accounts.count(uid)){ invalid(); continue; }
            Account a; a.user_id=uid; a.password=pwd; a.username=uname; a.privilege=pv; accounts[uid]=a; save_accounts(); push_log("useradd "+uid);
        }
        else if(cmd=="delete"){
            if(parts.size()!=2){ invalid(); continue; }
            if(current_priv()<7){ invalid(); continue; }
            string uid=parts[1]; if(!is_legal_id(uid)){ invalid(); continue; }
            if(!accounts.count(uid)){ invalid(); continue; }
            for(auto &s: session){ if(s.user_id==uid){ invalid(); goto cont; } }
            accounts.erase(uid); save_accounts(); push_log("delete "+uid);
            cont:;
        }
        else if(cmd=="show"){
            if(parts.size()==2 && parts[1]=="finance"){
                if(current_priv()<7){ invalid(); continue; }
                long long income=0, expend=0; for(auto &it: finances){ income+=it.income_cents; expend+=it.expenditure_cents; }
                cout<<"+ "<<format_price(income)<<" - "<<format_price(expend)<<"\n";
            }else if(parts.size()==3 && parts[1]=="finance"){
                if(current_priv()<7){ invalid(); continue; }
                long long cnt; if(!parse_positive_int(parts[2], cnt)){ if(parts[2]=="0"){ cout<<"\n"; } else invalid(); continue; }
                if(cnt>(long long)finances.size()){ invalid(); continue; }
                long long income=0, expend=0; for(long long i=(long long)finances.size()-cnt;i<(long long)finances.size();++i){ income+=finances[i].income_cents; expend+=finances[i].expenditure_cents; }
                cout<<"+ "<<format_price(income)<<" - "<<format_price(expend)<<"\n";
            }else{
                if(current_priv()<1){ invalid(); continue; }
                // book show with optional filter
                string filter_key; string filter_val; bool has_filter=false;
                if(parts.size()>=2){
                    // permit single token: -ISBN=xxx or -name="xxx"
                    if(parts.size()!=2){ invalid(); continue; }
                    string t=parts[1];
                    size_t eq=t.find('='); if(eq==string::npos){ invalid(); continue; }
                    filter_key=t.substr(0,eq); filter_val=t.substr(eq+1);
                    has_filter=true;
                    if(filter_key=="-ISBN"){ if(!is_legal_isbn(filter_val)) { invalid(); continue; } }
                    else if(filter_key=="-name"){ if(!is_legal_name_author(filter_val)) { invalid(); continue; } }
                    else if(filter_key=="-author"){ if(!is_legal_name_author(filter_val)) { invalid(); continue; } }
                    else if(filter_key=="-keyword"){ // must be single keyword (no '|')
                        if(filter_val.find('|')!=string::npos || !is_legal_keyword_one(filter_val)) { invalid(); continue; }
                    } else { invalid(); continue; }
                }
                // collect and sort by isbn
                vector<Book> list; list.reserve(books.size());
                for(auto &p: books){ const Book &b=p.second; if(!has_filter) list.push_back(b); else{
                        if(filter_key=="-ISBN" && b.isbn==filter_val) list.push_back(b);
                        else if(filter_key=="-name" && b.name==filter_val) list.push_back(b);
                        else if(filter_key=="-author" && b.author==filter_val) list.push_back(b);
                        else if(filter_key=="-keyword"){
                            for(auto &kw: b.keywords){ if(kw==filter_val){ list.push_back(b); break; } }
                        }
                    } }
                sort(list.begin(), list.end(), [](const Book&a,const Book&b){ return a.isbn<b.isbn; });
                if(list.empty()){ cout<<"\n"; continue; }
                for(size_t i=0;i<list.size();++i){ const Book &b=list[i];
                    string kw; for(size_t j=0;j<b.keywords.size();++j){ if(j) kw.push_back('|'); kw+=b.keywords[j]; }
                    cout<<b.isbn<<"\t"<<b.name<<"\t"<<b.author<<"\t"<<kw<<"\t"<<fixed<<setprecision(2)<<b.price<<"\t"<<b.stock<<"\n";
                }
            }
        }
        else if(cmd=="buy"){
            if(current_priv()<1){ invalid(); continue; }
            if(parts.size()!=3){ invalid(); continue; }
            string isbn=parts[1]; long long qty; if(!is_legal_isbn(isbn) || !parse_positive_int(parts[2], qty)){ invalid(); continue; }
            auto it=books.find(isbn); if(it==books.end()){ invalid(); continue; }
            if(qty>it->second.stock){ invalid(); continue; }
            long long price_cents = llround(it->second.price*100.0);
            long long cost = price_cents * qty;
            it->second.stock -= qty; save_books();
            finances.push_back({cost,0}); save_fin();
            cout<<format_price(cost)<<"\n";
            push_log("buy "+isbn+" "+to_string(qty));
        }
        else if(cmd=="select"){
            if(current_priv()<3){ invalid(); continue; }
            if(parts.size()!=2){ invalid(); continue; }
            string isbn=parts[1]; if(!is_legal_isbn(isbn)){ invalid(); continue; }
            session.back().selected_isbn.clear();
            auto it=books.find(isbn); if(it==books.end()){ Book b; b.isbn=isbn; books[isbn]=b; save_books(); }
            session.back().selected_isbn=isbn; push_log("select "+isbn);
        }
        else if(cmd=="modify"){
            if(current_priv()<3){ invalid(); continue; }
            string sel = current_selected(); if(sel.empty()){ invalid(); continue; }
            if(parts.size()<2){ invalid(); continue; }
            // detect duplicates of keys
            set<string> seen;
            // parse each -key=value token; values for name/author/keyword may contain spaces originally, but our split preserved quotes
            vector<pair<string,string>> kvs;
            for(size_t i=1;i<parts.size();++i){ string t=parts[i]; size_t eq=t.find('='); if(eq==string::npos){ invalid(); goto endmodify; }
                string k=t.substr(0,eq), v=t.substr(eq+1);
                if(seen.count(k)){ invalid(); goto endmodify; }
                seen.insert(k); kvs.push_back({k,v});
            }
            {
                Book &b = books[sel];
                for(auto &kv: kvs){ const string &k=kv.first; const string &v=kv.second;
                    if(k=="-ISBN"){
                        if(!is_legal_isbn(v)) { invalid(); goto endmodify; }
                        if(v==sel) { invalid(); goto endmodify; }
                        if(books.count(v)) { invalid(); goto endmodify; }
                        // move record
                        Book nb=b; nb.isbn=v; books.erase(sel); books[v]=nb; session.back().selected_isbn=v;
                    }else if(k=="-name"){
                        if(!is_legal_name_author(v)) { invalid(); goto endmodify; }
                        b.name=v;
                    }else if(k=="-author"){
                        if(!is_legal_name_author(v)) { invalid(); goto endmodify; }
                        b.author=v;
                    }else if(k=="-keyword"){
                        if(!is_legal_keyword_one(v)) { invalid(); goto endmodify; }
                        // split by |
                        vector<string> segs; string cur; stringstream ss(v); while(getline(ss,cur,'|')){ if(cur.empty()){ invalid(); goto endmodify; } segs.push_back(cur); }
                        // duplicate check
                        set<string> uniq; for(auto &s: segs){ if(uniq.count(s)){ invalid(); goto endmodify; } uniq.insert(s); }
                        b.keywords = segs;
                    }else if(k=="-price"){
                        long long cents; if(!parse_price(v, cents) || cents<=0){ invalid(); goto endmodify; }
                        b.price = cents/100.0;
                    }else{ invalid(); goto endmodify; }
                }
                save_books(); push_log("modify "+session.back().selected_isbn);
            }
            endmodify:;
        }
        else if(cmd=="import"){
            if(current_priv()<3){ invalid(); continue; }
            string sel = current_selected(); if(sel.empty()){ invalid(); continue; }
            if(parts.size()!=3){ invalid(); continue; }
            long long qty; if(!parse_positive_int(parts[1], qty)){ invalid(); continue; }
            long long cents; if(!parse_price(parts[2], cents) || cents<=0){ invalid(); continue; }
            Book &b = books[sel]; b.stock += qty; save_books(); finances.push_back({0,cents}); save_fin(); push_log("import "+sel+" "+to_string(qty));
        }
        else if(cmd=="log"){
            if(current_priv()<7){ invalid(); continue; }
            for(auto &l: opslog){ cout<<l<<"\n"; }
        }
        else if(cmd=="report"){
            if(current_priv()<7){ invalid(); continue; }
            if(parts.size()!=2){ invalid(); continue; }
            if(parts[1]=="finance"){
                long long income=0, expend=0; for(auto &it: finances){ income+=it.income_cents; expend+=it.expenditure_cents; }
                cout<<"Finance Report\nTotal Income: "<<format_price(income)<<"\nTotal Expenditure: "<<format_price(expend)<<"\n";
            }else if(parts[1]=="employee"){
                unordered_map<string,int> ops; for(auto &l: opslog){ // crude count per user by parsing ops? logs don't include user consistently; skip
                }
                cout<<"Employee Report\nEntries: "<<opslog.size()<<"\n";
            }else{ invalid(); }
        }
        else{
            invalid();
        }
    }
    // on exit, persist logs
    save_log();
    return 0;
}
