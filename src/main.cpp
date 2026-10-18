#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <filesystem>
#include <optional>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace ksl {
struct Loc { int line = 1, col = 1; };
struct Error : std::runtime_error {
  std::string category; Loc loc;
  Error(std::string kind, Loc at, std::string msg)
      : std::runtime_error(std::move(msg)), category(std::move(kind)), loc(at) {}
};
enum class T {
  End, Newline, Number, String, Character, Name, LParen, RParen, LSquare, RSquare,
  Less, Greater, LessEq, GreaterEq, Equal, NotEqual, Assign,
  Plus, Minus, Star, Slash, Percent, PlusEq, MinusEq, StarEq, SlashEq,
  Inc, Dec, Bang, And, Or,
  If, Else, While, Whileif, Loop, Fn, Return, Break, Continue,
  True, False, Null, Comma
};
struct Token { T type; std::string text; Loc loc; };
class Lexer {
  const std::string &src; size_t pos = 0; Loc loc;
  char peek(size_t n = 0) const { return pos+n < src.size() ? src[pos+n] : '\0'; }
  char take() { char c=peek(); ++pos; if(c=='\n') {++loc.line; loc.col=1;} else ++loc.col; return c; }
public:
  explicit Lexer(const std::string &s):src(s){}
  std::vector<Token> scan() {
    std::vector<Token> out;
    const std::unordered_map<std::string,T> words = {
      {"if",T::If},{"else",T::Else},{"while",T::While},{"whileif",T::Whileif},
      {"loop",T::Loop},{"fn",T::Fn},{"return",T::Return},{"break",T::Break},
      {"continue",T::Continue},{"true",T::True},{"false",T::False},
      {"null",T::Null},{"and",T::And},{"or",T::Or},{"not",T::Bang}};
    while(pos<src.size()) {
      Loc at=loc; char c=peek();
      if(c==' '||c=='\t'||c=='\r') {take(); continue;}
      if(c=='\n') {take(); out.push_back({T::Newline,"\\n",at}); continue;}
      if(c=='/'&&peek(1)=='/') {while(peek()&&peek()!='\n') take(); continue;}
      if((c>='0'&&c<='9') || (c=='.'&&peek(1)>='0'&&peek(1)<='9')) {
        std::string s;
        while(peek()>='0'&&peek()<='9') s+=take();
        if(peek()=='.') {s+=take(); while(peek()>='0'&&peek()<='9') s+=take();}
        out.push_back({T::Number,s,at}); continue;
      }
      if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_') {
        std::string s;
        while((peek()>='a'&&peek()<='z')||(peek()>='A'&&peek()<='Z')||
              (peek()>='0'&&peek()<='9')||peek()=='_') s+=take();
        auto it=words.find(s); out.push_back({it==words.end()?T::Name:it->second,s,at}); continue;
      }
      if(c=='"'||c=='\'') {
        char quote=take(); std::string s; bool closed=false;
        while(peek()) {
          if(peek()==quote) {take(); closed=true; break;}
          if(peek()=='\n') throw Error("SyntaxError",at,"unterminated string");
          if(peek()=='\\') {
            take(); char e=peek(); if(!e) break;
            if(e=='n') s+='\n'; else if(e=='t') s+='\t';
            else if(e=='"'||e=='\''||e=='\\') s+=e;
            else throw Error("SyntaxError",loc,"unknown string escape");
            take();
          } else s+=take();
        }
        if(!closed) throw Error("SyntaxError",at,"unterminated string");
        if(quote=='\''&&s.size()!=1)
          throw Error("SyntaxError",at,"let literal must contain exactly one byte");
        out.push_back({quote=='\''?T::Character:T::String,s,at}); continue;
      }
      std::string two{c,peek(1)};
      const std::unordered_map<std::string,T> pairs = {
        {"<=",T::LessEq},{">=",T::GreaterEq},{"==",T::Equal},{"!=",T::NotEqual},
        {"+=",T::PlusEq},{"-=",T::MinusEq},{"*=",T::StarEq},{"/=",T::SlashEq},
        {"++",T::Inc},{"--",T::Dec}};
      auto it=pairs.find(two);
      if(it!=pairs.end()) {take();take();out.push_back({it->second,two,at});continue;}
      const std::unordered_map<char,T> singles = {
        {'(',T::LParen},{')',T::RParen},{'[',T::LSquare},{']',T::RSquare},
        {'<',T::Less},{'>',T::Greater},{'=',T::Assign},{'+',T::Plus},
        {'-',T::Minus},{'*',T::Star},{'/',T::Slash},{'%',T::Percent},
        {'!',T::Bang},{',',T::Comma}};
      auto one=singles.find(c);
      if(one==singles.end()) throw Error("SyntaxError",at,std::string("unexpected character '")+c+"'");
      take(); out.push_back({one->second,std::string(1,c),at});
    }
    out.push_back({T::End,"",loc}); return out;
  }
};

struct Expr {
  enum Kind { Literal, Variable, Array, Unary, Binary, Assign, Postfix, Call, Index } kind;
  Token token; std::vector<std::unique_ptr<Expr>> args;
};
struct Stmt {
  enum Kind { Expression, Block, IfStmt, WhileStmt, WhileifStmt, LoopStmt,
              Function, ReturnStmt, BreakStmt, ContinueStmt } kind;
  Token token; std::vector<std::unique_ptr<Expr>> expressions;
  std::vector<std::unique_ptr<Stmt>> body, otherwise;
  std::vector<std::string> params;
};
using E = std::unique_ptr<Expr>; using S = std::unique_ptr<Stmt>;
class Parser {
  const std::vector<Token> &ts; size_t p=0; bool inAngle=false;
  int expressionDepth=0, statementDepth=0;
  struct DepthGuard {
    int &depth;
    DepthGuard(int &value,Loc at,const char *kind):depth(value) {
      if(depth>=64)
        throw Error("SyntaxError",at,std::string(kind)+" nesting exceeds 64 levels");
      ++depth;
    }
    ~DepthGuard() {--depth;}
  };
  const Token &cur() const {return ts[p];}
  const Token &prev() const {return ts[p-1];}
  bool is(T t) const {return cur().type==t;}
  bool match(T t) {if(is(t)){++p;return true;}return false;}
  Token expect(T t,const std::string &msg) {if(!is(t)) throw Error("SyntaxError",cur().loc,msg);return ts[p++];}
  void lines() {while(match(T::Newline)){} }
  void finish() {if(is(T::Newline)) lines(); else if(!is(T::RSquare)&&!is(T::End))
    throw Error("SyntaxError",cur().loc,"expected a newline after statement");}
  static E node(Expr::Kind k,Token t) {auto n=std::make_unique<Expr>();n->kind=k;n->token=t;return n;}
  static S stmt(Stmt::Kind k,Token t) {auto n=std::make_unique<Stmt>();n->kind=k;n->token=t;return n;}
  static int precedence(T t) {
    switch(t) {
      case T::Assign:case T::PlusEq:case T::MinusEq:case T::StarEq:case T::SlashEq:return 1;
      case T::Or:return 2; case T::And:return 3;
      case T::Equal:case T::NotEqual:return 4;
      case T::Less:case T::Greater:case T::LessEq:case T::GreaterEq:return 5;
      case T::Plus:case T::Minus:return 6;
      case T::Star:case T::Slash:case T::Percent:return 7;
      default:return 0;
    }
  }
  static bool begins(T t) {
    return t==T::Number||t==T::String||t==T::Character||t==T::Name||t==T::True||t==T::False||
      t==T::Null||t==T::LParen||t==T::LSquare||t==T::Minus||t==T::Bang||t==T::Plus;
  }
  bool squareContinuesExpression() const {
    size_t depth=0;
    for(size_t i=p+1;i<ts.size();++i) {
      if(ts[i].type==T::LSquare)++depth;
      else if(ts[i].type==T::RSquare && --depth==0) {
        T next=ts[i+1].type;
        return next==T::Greater||next==T::LSquare||next==T::RParen||next==T::Comma||
          next==T::Plus||next==T::Minus||next==T::Star||next==T::Slash||
          next==T::Percent||next==T::Equal||next==T::NotEqual||next==T::Less||
          next==T::LessEq||next==T::GreaterEq||next==T::And||next==T::Or;
      }
    }
    return false;
  }
  E primary() {
    Token t=cur();
    if(match(T::Number)||match(T::String)||match(T::Character)||match(T::True)||match(T::False)||match(T::Null)) return node(Expr::Literal,t);
    if(match(T::Name)) return node(Expr::Variable,t);
    if(match(T::Minus)||match(T::Bang)||match(T::Plus)) {
      auto n=node(Expr::Unary,t);n->args.push_back(expression(8));return n;
    }
    if(match(T::LParen)) {
      lines();auto n=expression();lines();
      expect(T::RParen,"expected ')' after expression");return n;
    }
    if(match(T::LSquare)) {
      auto n=node(Expr::Array,t);
      lines();
      while(!is(T::RSquare)&&!is(T::End)) {
        n->args.push_back(expression());lines();
        if(!match(T::Comma))break;
        lines();
      }
      expect(T::RSquare,"expected ']' after array elements");return n;
    }
    throw Error("SyntaxError",t.loc,"expected an expression");
  }
  E expression(int min=1) {
    DepthGuard guard(expressionDepth,cur().loc,"expression");
    E left=primary();
    while(true) {
      if(match(T::LParen)) {
        Token op=prev();auto n=node(Expr::Call,op);n->args.push_back(std::move(left));
        lines();
        while(!is(T::RParen)&&!is(T::End)) {
          n->args.push_back(expression());lines();
          if(!match(T::Comma))break;
          lines();
        }
        expect(T::RParen,"expected ')' after arguments");left=std::move(n);continue;
      }
      if(match(T::LSquare)) {
        Token op=prev();auto n=node(Expr::Index,op);n->args.push_back(std::move(left));
        lines();n->args.push_back(expression());lines();
        expect(T::RSquare,"expected ']' after index");
        left=std::move(n);continue;
      }
      if(is(T::Inc)||is(T::Dec)) {
        if(left->kind!=Expr::Variable)
          throw Error("SyntaxError",cur().loc,"++ and -- require a variable");
        Token op=ts[p++];auto n=node(Expr::Postfix,op);n->args.push_back(std::move(left));left=std::move(n);continue;
      }
      int prec=precedence(cur().type);
      // A final '>' in a control expression has no expression on its right.
      if(prec<min || (is(T::Greater) &&
          (!begins(ts[p+1].type) ||
           (inAngle && ts[p+1].type==T::LSquare && !squareContinuesExpression())))) break;
      Token op=ts[p++]; bool assignment=prec==1;
      if(assignment && left->kind!=Expr::Variable && left->kind!=Expr::Index)
        throw Error("SyntaxError",left->token.loc,"assignment target must be a variable or array element");
      auto n=node(assignment?Expr::Assign:Expr::Binary,op);
      n->args.push_back(std::move(left));n->args.push_back(expression(prec+(assignment?0:1)));
      left=std::move(n);
    }
    return left;
  }
  E angle() {expect(T::Less,"expected '<' before control expression");
    inAngle=true;auto e=expression();inAngle=false;
    expect(T::Greater,"expected '>' after control expression");return e;}
  std::vector<S> block() {
    expect(T::LSquare,"expected '[' to start block");lines();std::vector<S> b;
    while(!is(T::RSquare)&&!is(T::End)) {lines();if(is(T::RSquare)||is(T::End))break;b.push_back(statement());}
    expect(T::RSquare,"expected ']' to close block");return b;
  }
  S statement() {
    DepthGuard guard(statementDepth,cur().loc,"statement");
    lines();Token t=cur();
    if(match(T::If)) {
      auto n=stmt(Stmt::IfStmt,t);n->expressions.push_back(angle());lines();n->body=block();
      bool separated=is(T::Newline);lines();
      if(match(T::Else)) {lines();n->otherwise=block();finish();}
      else if(!separated)finish();
      return n;
    }
    if(match(T::While)||match(T::Whileif)) {
      auto n=stmt(t.type==T::While?Stmt::WhileStmt:Stmt::WhileifStmt,t);
      n->expressions.push_back(angle());lines();
      if(n->kind==Stmt::WhileifStmt) {n->expressions.push_back(angle());lines();}
      n->body=block();finish();return n;
    }
    if(match(T::Loop)) {auto n=stmt(Stmt::LoopStmt,t);lines();n->body=block();finish();return n;}
    if(match(T::Fn)) {
      auto n=stmt(Stmt::Function,expect(T::Name,"expected function name"));
      expect(T::LParen,"expected '(' after function name");lines();
      while(!is(T::RParen)&&!is(T::End)) {
        Token param=expect(T::Name,"expected parameter name");
        for(const auto &existing:n->params)
          if(existing==param.text)
            throw Error("SyntaxError",param.loc,"duplicate parameter '"+param.text+"'");
        n->params.push_back(param.text);
        lines();if(!match(T::Comma))break;
        lines();
      }
      expect(T::RParen,"expected ')' after parameters");lines();n->body=block();finish();return n;
    }
    if(match(T::Return)) {
      auto n=stmt(Stmt::ReturnStmt,t);
      if(!is(T::Newline)&&!is(T::RSquare)&&!is(T::End))
        n->expressions.push_back(expression());
      finish();return n;
    }
    if(match(T::Break)||match(T::Continue)) {
      auto n=stmt(t.type==T::Break?Stmt::BreakStmt:Stmt::ContinueStmt,t);finish();return n;
    }
    auto n=stmt(Stmt::Expression,t);n->expressions.push_back(expression());finish();return n;
  }
public:
  explicit Parser(const std::vector<Token> &tokens):ts(tokens){}
  std::vector<S> parse() {std::vector<S> out;lines();while(!is(T::End)) {out.push_back(statement());lines();}return out;}
};

struct Function; struct ArrayValue;
using Value=std::variant<std::monostate,std::int32_t,std::int64_t,double,std::string,char,bool,
                         std::shared_ptr<ArrayValue>,std::shared_ptr<Function>>;
struct ArrayValue {std::vector<Value> elements;};
bool isInteger(const Value &v) {
  return std::holds_alternative<std::int32_t>(v)||std::holds_alternative<std::int64_t>(v);
}
std::int64_t integer(const Value &v) {
  if(auto n=std::get_if<std::int32_t>(&v))return *n;
  return std::get<std::int64_t>(v);
}
Value makeInteger(std::int64_t n) {
  if(n>=std::numeric_limits<std::int32_t>::min() && n<=std::numeric_limits<std::int32_t>::max())
    return static_cast<std::int32_t>(n);
  return n;
}
bool isNumeric(const Value &v) {return isInteger(v)||std::holds_alternative<double>(v);}
double numeric(const Value &v) {
  if(auto n=std::get_if<double>(&v))return *n;
  return static_cast<double>(integer(v));
}
// Return -1, 0, 1, or 2 (unordered NaN), without rounding a bnum to double.
int compareNumbers(const Value &a,const Value &b) {
  if(isInteger(a)&&isInteger(b)) {
    auto x=integer(a),y=integer(b);return x<y?-1:x>y?1:0;
  }
  if(!isInteger(a)&&!isInteger(b)) {
    double x=std::get<double>(a),y=std::get<double>(b);
    return std::isnan(x)||std::isnan(y)?2:x<y?-1:x>y?1:0;
  }
  bool reverse=!isInteger(a);
  auto i=integer(reverse?b:a);
  double d=std::get<double>(reverse?a:b);
  if(std::isnan(d))return 2;
  int result;
  if(d>=9223372036854775808.0)result=-1;
  else if(d< -9223372036854775808.0)result=1;
  else {
    auto whole=static_cast<std::int64_t>(d);
    result=i<whole?-1:i>whole?1:double(whole)<d?-1:double(whole)>d?1:0;
  }
  return reverse?-result:result;
}
std::string typeName(const Value &v) {
  if(std::holds_alternative<std::monostate>(v))return "null";
  if(std::holds_alternative<std::int32_t>(v))return "num";
  if(std::holds_alternative<std::int64_t>(v))return "bnum";
  if(std::holds_alternative<double>(v))return "dec";
  if(std::holds_alternative<std::string>(v))return "word";
  if(std::holds_alternative<char>(v))return "let";
  if(std::holds_alternative<bool>(v))return "bool";
  if(std::holds_alternative<std::shared_ptr<ArrayValue>>(v))return "array";
  return "function";
}
struct Env:std::enable_shared_from_this<Env> {
  std::unordered_map<std::string,Value> values;std::shared_ptr<Env> parent;
  explicit Env(std::shared_ptr<Env> p=nullptr):parent(std::move(p)){}
  Value get(const std::string &s,Loc at) {
    for(const Env *scope=this;scope;scope=scope->parent.get()) {
      auto it=scope->values.find(s);
      if(it!=scope->values.end())return it->second;
    }
    std::string suggestion;
    size_t best=s.size()<=3?1:2;
    for(const Env *scope=this;scope;scope=scope->parent.get()) {
      for(const auto &entry:scope->values) {
        const std::string &name=entry.first;
        std::vector<size_t> previous(name.size()+1),current(name.size()+1);
        for(size_t j=0;j<=name.size();++j)previous[j]=j;
        for(size_t i=1;i<=s.size();++i) {
          current[0]=i;
          for(size_t j=1;j<=name.size();++j) {
            size_t edit=s[i-1]==name[j-1]?0:1;
            current[j]=std::min({previous[j]+1,current[j-1]+1,previous[j-1]+edit});
          }
          previous.swap(current);
        }
        size_t distance=previous[name.size()];
        if(distance<=best&&(suggestion.empty()||distance<best||name<suggestion)) {
          best=distance;suggestion=name;
        }
      }
    }
    std::string message="'"+s+"' is not defined";
    if(!suggestion.empty())message+="; did you mean '"+suggestion+"'?";
    throw Error("UndefinedVariable",at,message);
  }
  void set(const std::string &s,Value v) {
    if(values.count(s)) {values[s]=std::move(v);return;}
    if(parent) {auto e=parent;while(e){if(e->values.count(s)){e->values[s]=std::move(v);return;}e=e->parent;}}
    values[s]=std::move(v);
  }
};
struct Function {const Stmt *declaration;std::shared_ptr<Env> closure;};
std::string displayValue(const Value &v,std::unordered_set<const ArrayValue *> &active) {
  if(std::holds_alternative<std::monostate>(v))return "null";
  if(auto n=std::get_if<std::int32_t>(&v))return std::to_string(*n);
  if(auto n=std::get_if<std::int64_t>(&v))return std::to_string(*n);
  if(auto n=std::get_if<double>(&v)) {std::ostringstream o;o.precision(15);o<<*n;return o.str();}
  if(auto s=std::get_if<std::string>(&v))return *s;
  if(auto c=std::get_if<char>(&v))return std::string(1,*c);
  if(auto b=std::get_if<bool>(&v))return *b?"true":"false";
  if(auto a=std::get_if<std::shared_ptr<ArrayValue>>(&v)) {
    if(active.count(a->get()))return "[<cycle>]";
    active.insert(a->get());
    std::string s="[";
    for(size_t i=0;i<(*a)->elements.size();++i) {
      if(i)s+=", ";
      s+=displayValue((*a)->elements[i],active);
    }
    active.erase(a->get());
    return s+"]";
  }
  return "<function>";
}
std::string display(const Value &v) {
  std::unordered_set<const ArrayValue *> active;
  return displayValue(v,active);
}
bool truth(const Value &v) {
  if(std::holds_alternative<std::monostate>(v))return false;
  if(isInteger(v))return integer(v)!=0;
  if(auto n=std::get_if<double>(&v))return *n!=0;
  if(auto s=std::get_if<std::string>(&v))return !s->empty();
  if(auto c=std::get_if<char>(&v))return *c!='\0';
  if(auto b=std::get_if<bool>(&v))return *b;
  if(auto a=std::get_if<std::shared_ptr<ArrayValue>>(&v))return !(*a)->elements.empty();
  return true;
}
bool equal(const Value &a,const Value &b) {
  if(isNumeric(a)&&isNumeric(b)) {
    return compareNumbers(a,b)==0;
  }
  if(a.index()!=b.index())return false;
  if(auto x=std::get_if<char>(&a))return *x==std::get<char>(b);
  if(auto x=std::get_if<double>(&a))return *x==std::get<double>(b);
  if(auto x=std::get_if<std::string>(&a))return *x==std::get<std::string>(b);
  if(auto x=std::get_if<bool>(&a))return *x==std::get<bool>(b);
  if(std::holds_alternative<std::monostate>(a))return true;
  if(auto x=std::get_if<std::shared_ptr<ArrayValue>>(&a))return *x==std::get<std::shared_ptr<ArrayValue>>(b);
  return std::get<std::shared_ptr<Function>>(a)==std::get<std::shared_ptr<Function>>(b);
}
struct ReturnSignal {Value value;};struct BreakSignal {};struct ContinueSignal {};
class Interpreter {
  std::shared_ptr<Env> global=std::make_shared<Env>();std::shared_ptr<Env> env=global;
  std::vector<std::vector<S>> programs; // Keeps function AST nodes alive between REPL entries.
  int loopDepth=0, functionDepth=0;
  double number(const Value &v,Loc at) {
    if(isNumeric(v))return numeric(v);
    throw Error("TypeError",at,"expected a number, got "+typeName(v));
  }
  size_t index(const Value &v,size_t size,Loc at) {
    if(!isNumeric(v))
      throw Error("TypeError",at,"array index must be an integer number, got "+typeName(v));
    double n=number(v,at);
    if(!std::isfinite(n)||std::floor(n)!=n)
      throw Error("TypeError",at,"array index must be an integer number");
    bool invalid=isInteger(v)?(integer(v)<0||static_cast<std::uint64_t>(integer(v))>=size)
                             :(n<0||n>=static_cast<double>(size));
    if(invalid) {
      std::ostringstream msg;msg<<"attempted index: "<<display(v)<<"; valid range: ";
      if(size)msg<<"0.."<<size-1;else msg<<"empty array";
      throw Error("ArrayIndexOutOfBounds",at,msg.str());
    }
    return isInteger(v)?static_cast<size_t>(integer(v)):static_cast<size_t>(n);
  }
  Value binary(T op,Value a,Value b,Loc at) {
    if(op==T::Equal)return equal(a,b);
    if(op==T::NotEqual)return !equal(a,b);
    if(isNumeric(a)&&isNumeric(b) &&
       (op==T::Less||op==T::Greater||op==T::LessEq||op==T::GreaterEq)) {
      int order=compareNumbers(a,b);
      if(op==T::Less)return order==-1;
      if(op==T::Greater)return order==1;
      if(op==T::LessEq)return order==0||order==-1;
      return order==0||order==1;
    }
    if(op==T::Plus&&(std::holds_alternative<std::string>(a)||std::holds_alternative<std::string>(b)||
                    std::holds_alternative<char>(a)||std::holds_alternative<char>(b)))
      return display(a)+display(b);
    if(isInteger(a)&&isInteger(b)&&op!=T::Slash) {
      std::int64_t x=integer(a),y=integer(b);
      const auto hi=std::numeric_limits<std::int64_t>::max();
      const auto lo=std::numeric_limits<std::int64_t>::min();
      bool overflow=false;
      if(op==T::Plus)overflow=(y>0&&x>hi-y)||(y<0&&x<lo-y);
      if(op==T::Minus)overflow=(y<0&&x>hi+y)||(y>0&&x<lo+y);
      if(op==T::Star)overflow=(x>0&&y>0&&x>hi/y)||
        (x>0&&y<0&&y<lo/x)||(x<0&&y>0&&x<lo/y)||
        (x<0&&y<0&&x<hi/y);
      if(overflow)throw Error("NumberOutOfRange",at,"integer arithmetic exceeds bnum range");
      if(op==T::Plus)return makeInteger(x+y);
      if(op==T::Minus)return makeInteger(x-y);
      if(op==T::Star)return makeInteger(x*y);
      if(op==T::Percent) {
        if(y==0)throw Error("DivisionByZero",at,"cannot take remainder by zero");
        if(x==lo&&y==-1)return makeInteger(0);
        return makeInteger(x%y);
      }
      if(op==T::Less)return x<y;
      if(op==T::Greater)return x>y;
      if(op==T::LessEq)return x<=y;
      if(op==T::GreaterEq)return x>=y;
    }
    double x=number(a,at),y=number(b,at);
    auto decimal=[&](double result)->Value {
      if(!std::isfinite(result))
        throw Error("NumberOutOfRange",at,"decimal arithmetic produced a non-finite result");
      return result;
    };
    switch(op) {
      case T::Plus:return decimal(x+y);case T::Minus:return decimal(x-y);
      case T::Star:return decimal(x*y);
      case T::Slash:if(y==0)throw Error("DivisionByZero",at,"cannot divide by zero");return decimal(x/y);
      case T::Percent:if(y==0)throw Error("DivisionByZero",at,"cannot take remainder by zero");return decimal(std::fmod(x,y));
      case T::Less:return x<y;case T::Greater:return x>y;
      case T::LessEq:return x<=y;case T::GreaterEq:return x>=y;
      default:throw Error("TypeError",at,"unsupported binary operator");
    }
  }
  Value evaluate(const Expr &e) {
    const auto &a=e.args;T op=e.token.type;
    switch(e.kind) {
      case Expr::Literal:
        if(op==T::Number) {
          try {
            if(e.token.text.find('.')!=std::string::npos) {
              double n=std::stod(e.token.text);
              if(!std::isfinite(n))throw std::out_of_range("decimal literal");
              return n;
            }
            return makeInteger(std::stoll(e.token.text));
          }
          catch(const std::out_of_range &) {
            throw Error("NumberOutOfRange",e.token.loc,"numeric literal is outside its supported range");
          }
        }
        if(op==T::String)return e.token.text;
        if(op==T::Character)return e.token.text[0];
        if(op==T::True)return true;
        if(op==T::False)return false;
        return {};
      case Expr::Variable:return env->get(e.token.text,e.token.loc);
      case Expr::Array: {auto v=std::make_shared<ArrayValue>();for(auto &x:a)v->elements.push_back(evaluate(*x));return v;}
      case Expr::Unary: {
        if(op==T::Minus&&a[0]->kind==Expr::Literal&&a[0]->token.type==T::Number&&
           a[0]->token.text=="9223372036854775808")
          return std::numeric_limits<std::int64_t>::min();
        Value v=evaluate(*a[0]);if(op==T::Bang)return !truth(v);
        if(isInteger(v)) {
          std::int64_t n=integer(v);
          if(op==T::Minus&&n==std::numeric_limits<std::int64_t>::min())
            throw Error("NumberOutOfRange",e.token.loc,"integer arithmetic exceeds bnum range");
          return makeInteger(op==T::Minus?-n:n);
        }
        double n=number(v,e.token.loc);return op==T::Minus?-n:n;
      }
      case Expr::Binary: {
        Value left=evaluate(*a[0]);
        if(op==T::And) return truth(left)?evaluate(*a[1]):left;
        if(op==T::Or) return truth(left)?left:evaluate(*a[1]);
        return binary(op,left,evaluate(*a[1]),e.token.loc);
      }
      case Expr::Index: {
        Value v=evaluate(*a[0]);Value ix=evaluate(*a[1]);
        auto arr=std::get_if<std::shared_ptr<ArrayValue>>(&v);
        if(!arr)throw Error("TypeError",e.token.loc,"indexing requires an array");
        return (*arr)->elements[index(ix,(*arr)->elements.size(),a[1]->token.loc)];
      }
      case Expr::Assign: {
        const Expr &target=*a[0];
        if(target.kind!=Expr::Variable&&target.kind!=Expr::Index)
          throw Error("SyntaxError",target.token.loc,"assignment target must be a variable or array element");
        Value rhs=evaluate(*a[1]);Value old;
        auto combine=[&](Value lhs)->Value {
          if(op==T::Assign)return rhs;
          T basic=op==T::PlusEq?T::Plus:op==T::MinusEq?T::Minus:op==T::StarEq?T::Star:T::Slash;
          return binary(basic,lhs,rhs,e.token.loc);
        };
        if(target.kind==Expr::Variable) {
          if(op!=T::Assign)old=env->get(target.token.text,target.token.loc);
          Value v=combine(old);env->set(target.token.text,v);return v;
        }
        Value base=evaluate(*target.args[0]);auto arr=std::get_if<std::shared_ptr<ArrayValue>>(&base);
        if(!arr)throw Error("TypeError",target.token.loc,"assignment index requires an array");
        size_t i=index(evaluate(*target.args[1]),(*arr)->elements.size(),target.args[1]->token.loc);
        Value v=combine((*arr)->elements[i]);(*arr)->elements[i]=v;return v;
      }
      case Expr::Postfix: {
        if(a[0]->kind!=Expr::Variable)throw Error("SyntaxError",e.token.loc,"++ and -- require a variable");
        const std::string &name=a[0]->token.text;Value old=env->get(name,e.token.loc);
        Value next=binary(op==T::Inc?T::Plus:T::Minus,old,makeInteger(1),e.token.loc);
        env->set(name,next);return old;
      }
      case Expr::Call: {
        std::vector<Value> args;for(size_t i=1;i<a.size();++i)args.push_back(evaluate(*a[i]));
        if(a[0]->kind==Expr::Variable) {
          const std::string &name=a[0]->token.text;
          if(name=="print") {for(size_t i=0;i<args.size();++i){if(i)std::cout<<' ';std::cout<<display(args[i]);}std::cout<<'\n';return {};}
          if(name=="input") {if(args.size()>1)throw Error("ArityError",e.token.loc,"input expects 0 or 1 argument");
            if(!args.empty())std::cout<<display(args[0])<<std::flush;
            std::string s;std::getline(std::cin,s);return s;}
          if(name=="len") {if(args.size()!=1)throw Error("ArityError",e.token.loc,"len expects 1 argument");
            if(auto s=std::get_if<std::string>(&args[0]))return makeInteger(static_cast<std::int64_t>(s->size()));
            if(auto v=std::get_if<std::shared_ptr<ArrayValue>>(&args[0]))
              return makeInteger(static_cast<std::int64_t>((*v)->elements.size()));
            throw Error("TypeError",e.token.loc,"len expects a string or array");}
          if(name=="type") {
            if(args.size()!=1)throw Error("ArityError",e.token.loc,"type expects 1 argument");
            return typeName(args[0]);
          }
        }
        Value callable=evaluate(*a[0]);auto f=std::get_if<std::shared_ptr<Function>>(&callable);
        if(!f)throw Error("TypeError",e.token.loc,"value is not callable");
        const Stmt &decl=*(*f)->declaration;
        if(args.size()!=decl.params.size())throw Error("ArityError",e.token.loc,
          "function '"+decl.token.text+"' expects "+std::to_string(decl.params.size())+
          " arguments, got "+std::to_string(args.size()));
        if(functionDepth>=64)
          throw Error("CallDepthExceeded",e.token.loc,
            "function call depth exceeded 64 while calling '"+decl.token.text+"'");
        auto saved=env;int savedLoops=loopDepth;
        env=std::make_shared<Env>((*f)->closure);loopDepth=0;++functionDepth;
        for(size_t i=0;i<args.size();++i)env->values[decl.params[i]]=args[i];
        Value result;
        try {executeBlock(decl.body);} catch(const ReturnSignal &r) {result=r.value;}
        catch(...) {env=saved;loopDepth=savedLoops;--functionDepth;throw;}
        env=saved;loopDepth=savedLoops;--functionDepth;return result;
      }
    }
    return {};
  }
  void executeBlock(const std::vector<S> &body) {for(auto &s:body)execute(*s);}
  void execute(const Stmt &s) {
    switch(s.kind) {
      case Stmt::Expression:evaluate(*s.expressions[0]);break;
      case Stmt::IfStmt:
        if(truth(evaluate(*s.expressions[0])))executeBlock(s.body);else executeBlock(s.otherwise);break;
      case Stmt::Function:env->set(s.token.text,std::make_shared<Function>(Function{&s,env}));break;
      case Stmt::ReturnStmt:
        if(!functionDepth)throw Error("ControlFlowError",s.token.loc,"return outside a function");
        throw ReturnSignal{s.expressions.empty()?Value{}:evaluate(*s.expressions[0])};
      case Stmt::BreakStmt:
        if(!loopDepth)throw Error("ControlFlowError",s.token.loc,"break outside a loop");
        throw BreakSignal{};
      case Stmt::ContinueStmt:
        if(!loopDepth)throw Error("ControlFlowError",s.token.loc,"continue outside a loop");
        throw ContinueSignal{};
      case Stmt::WhileStmt:case Stmt::WhileifStmt:case Stmt::LoopStmt: {
        ++loopDepth;
        try {
          while(s.kind==Stmt::LoopStmt||truth(evaluate(*s.expressions[0]))) {
            try {executeBlock(s.body);}catch(const ContinueSignal &) {}
            if(s.kind==Stmt::WhileifStmt)evaluate(*s.expressions[1]);
          }
        } catch(const BreakSignal &) {} catch(...) {--loopDepth;throw;}
        --loopDepth;break;
      }
      case Stmt::Block:break;
    }
  }
public:
  void run(std::vector<S> program) {
    programs.push_back(std::move(program));
    executeBlock(programs.back());
  }
};
void report(const Error &e,const std::string &filename,const std::string &source) {
  std::cerr<<"KSL "<<(e.category=="SyntaxError"?"Syntax":"Runtime")<<" Error: "<<e.category<<"\n\n";
  std::cerr<<filename<<":"<<e.loc.line<<":"<<e.loc.col<<"\n";
  std::istringstream in(source);std::string line;
  for(int i=1;i<=e.loc.line&&std::getline(in,line);++i)if(i==e.loc.line) {
    std::cerr<<"    "<<line<<"\n    "<<std::string(e.loc.col>0?e.loc.col-1:0,' ')<<"^\n";
  }
  std::cerr<<"\n"<<e.what()<<"\n";
}
int executeSource(const std::string &source,const std::string &name,Interpreter &runtime,
                  bool allowIncomplete=false) {
  std::vector<Token> tokens;
  try {tokens=Lexer(source).scan();auto program=Parser(tokens).parse();runtime.run(std::move(program));return 0;}
  catch(const Error &e) {
    if(allowIncomplete&&e.category=="SyntaxError"&&!tokens.empty()&&
       e.loc.line==tokens.back().loc.line&&e.loc.col==tokens.back().loc.col)
      return 3;
    report(e,name,source);return 1;
  }
  catch(const std::exception &e) {std::cerr<<"KSL Internal Error: "<<e.what()<<"\n";return 2;}
}
int checkSource(const std::string &source,const std::string &name) {
  try {auto tokens=Lexer(source).scan();Parser(tokens).parse();return 0;}
  catch(const Error &e) {report(e,name,source);return 1;}
  catch(const std::exception &e) {std::cerr<<"KSL Internal Error: "<<e.what()<<"\n";return 2;}
}
} // namespace ksl
namespace {
// A packaged program is the interpreter followed by source, UTF-8 name,
// two little-endian lengths and a fixed trailer. The operating system ignores
// bytes appended after its executable image.
std::optional<std::pair<std::string,std::string>> packagedProgram() {
  std::filesystem::path path;
#ifdef _WIN32
  std::wstring wide(32768,L'\0');
  DWORD n=GetModuleFileNameW(nullptr,wide.data(),static_cast<DWORD>(wide.size()));
  if(!n||n>=wide.size()) return std::nullopt;
  wide.resize(n);path=wide;
#else
  path="/proc/self/exe";
#endif
  std::ifstream file(path,std::ios::binary|std::ios::ate);
  if(!file)return std::nullopt;
  auto size=file.tellg();
  if(size<std::streamoff(24))return std::nullopt;
  file.seekg(size-std::streamoff(24));
  unsigned char tail[24]{};
  if(!file.read(reinterpret_cast<char *>(tail),24))return std::nullopt;
  if(std::string(reinterpret_cast<char *>(tail+16),8)!="KSLPKG01")return std::nullopt;
  auto length=[&](int offset) {
    std::uint64_t value=0;
    for(int i=7;i>=0;--i)value=(value<<8)|tail[offset+i];
    return value;
  };
  auto sourceLength=length(0),nameLength=length(8);
  if(sourceLength>64*1024*1024||nameLength>4096||
     sourceLength+nameLength>static_cast<std::uint64_t>(size)-24)
    return std::nullopt;
  file.seekg(size-std::streamoff(24+sourceLength+nameLength));
  std::string source(static_cast<size_t>(sourceLength),'\0');
  std::string name(static_cast<size_t>(nameLength),'\0');
  if(!file.read(source.data(),static_cast<std::streamsize>(source.size()))||
     !file.read(name.data(),static_cast<std::streamsize>(name.size())))
    return std::nullopt;
  return std::make_pair(std::move(source),std::move(name));
}
}
#ifdef KSL_EMBEDDED_PROGRAM
int main(int argc,char **argv) {
  if(argc!=1) {std::cerr<<"This compiled KSL program takes no command-line arguments.\n";return 2;}
  std::string source(reinterpret_cast<const char *>(kslEmbeddedSource),sizeof(kslEmbeddedSource)-1);
  std::string name(reinterpret_cast<const char *>(kslEmbeddedName),sizeof(kslEmbeddedName)-1);
  ksl::Interpreter runtime;
  return ksl::executeSource(source,name,runtime);
}
#else
int main(int argc,char **argv) {
  if(auto program=packagedProgram()) {
    ksl::Interpreter runtime;
    return ksl::executeSource(program->first,program->second,runtime);
  }
  bool checking=argc==3&&std::string(argv[1])=="--check";
  if(argc>2&&!checking) {std::cerr<<"Usage: ksl [file.ksl] or ksl --check file.ksl\n";return 2;}
  if(argc==2 && std::string(argv[1])=="--help") {
    std::cout<<"KSL v0.1 — Kodexically Slow Language\n"
             <<"Usage: ksl [file.ksl] or ksl --check file.ksl\n"
             <<"With no file, start the REPL.\n";
    return 0;
  }
  if(argc==2 && std::string(argv[1])=="--version") {
    std::cout<<"KSL 0.1.0\n";
    return 0;
  }
  ksl::Interpreter runtime;
  if(argc==1) {
    std::cout<<"KSL — Kodexically Slow Language v0.1\n";
    std::string line,source;
    while(std::cout<<(source.empty()?"> ":"... ")&&std::getline(std::cin,line)) {
      source+=line+"\n";
      if(ksl::executeSource(source,"<repl>",runtime,true)!=3)source.clear();
    }
    if(!source.empty())ksl::executeSource(source,"<repl>",runtime);
    return 0;
  }
  const char *path=checking?argv[2]:argv[1];
  std::ifstream f(path);if(!f){std::cerr<<"KSL File Error: cannot open '"<<path<<"'\n";return 2;}
  std::ostringstream buffer;buffer<<f.rdbuf();
  return checking?ksl::checkSource(buffer.str(),path):ksl::executeSource(buffer.str(),path,runtime);
}
#endif
