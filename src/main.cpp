#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace ksl {
struct Loc { int line = 1, col = 1; };
struct Error : std::runtime_error {
  std::string category; Loc loc;
  Error(std::string kind, Loc at, std::string msg)
      : std::runtime_error(std::move(msg)), category(std::move(kind)), loc(at) {}
};
enum class T {
  End, Newline, Number, String, Name, LParen, RParen, LSquare, RSquare,
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
    while(peek()) {
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
      if(c=='"') {
        take(); std::string s; bool closed=false;
        while(peek()) {
          if(peek()=='"') {take(); closed=true; break;}
          if(peek()=='\n') throw Error("SyntaxError",at,"unterminated string");
          if(peek()=='\\') {
            take(); char e=peek(); if(!e) break;
            if(e=='n') s+='\n'; else if(e=='t') s+='\t';
            else if(e=='"'||e=='\\') s+=e;
            else throw Error("SyntaxError",loc,"unknown string escape");
            take();
          } else s+=take();
        }
        if(!closed) throw Error("SyntaxError",at,"unterminated string");
        out.push_back({T::String,s,at}); continue;
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
  const std::vector<Token> &ts; size_t p=0;
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
    return t==T::Number||t==T::String||t==T::Name||t==T::True||t==T::False||
      t==T::Null||t==T::LParen||t==T::LSquare||t==T::Minus||t==T::Bang||t==T::Plus;
  }
  E primary() {
    Token t=cur();
    if(match(T::Number)||match(T::String)||match(T::True)||match(T::False)||match(T::Null)) return node(Expr::Literal,t);
    if(match(T::Name)) return node(Expr::Variable,t);
    if(match(T::Minus)||match(T::Bang)||match(T::Plus)) {
      auto n=node(Expr::Unary,t);n->args.push_back(expression(8));return n;
    }
    if(match(T::LParen)) {auto n=expression();expect(T::RParen,"expected ')' after expression");return n;}
    if(match(T::LSquare)) {
      auto n=node(Expr::Array,t);
      if(!is(T::RSquare)) do {n->args.push_back(expression());} while(match(T::Comma));
      expect(T::RSquare,"expected ']' after array elements");return n;
    }
    throw Error("SyntaxError",t.loc,"expected an expression");
  }
  E expression(int min=1) {
    E left=primary();
    while(true) {
      if(match(T::LParen)) {
        Token op=prev();auto n=node(Expr::Call,op);n->args.push_back(std::move(left));
        if(!is(T::RParen)) do {n->args.push_back(expression());} while(match(T::Comma));
        expect(T::RParen,"expected ')' after arguments");left=std::move(n);continue;
      }
      if(match(T::LSquare)) {
        Token op=prev();auto n=node(Expr::Index,op);n->args.push_back(std::move(left));
        n->args.push_back(expression());expect(T::RSquare,"expected ']' after index");
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
          (ts[p+1].type==T::LSquare || !begins(ts[p+1].type)))) break;
      Token op=ts[p++]; bool assignment=prec==1;
      if(assignment && left->kind!=Expr::Variable && left->kind!=Expr::Index)
        throw Error("SyntaxError",left->token.loc,"assignment target must be a variable or array element");
      auto n=node(assignment?Expr::Assign:Expr::Binary,op);
      n->args.push_back(std::move(left));n->args.push_back(expression(prec+(assignment?0:1)));
      left=std::move(n);
    }
    return left;
  }
  E angle() {expect(T::Less,"expected '<' before control expression");auto e=expression();
    expect(T::Greater,"expected '>' after control expression");return e;}
  std::vector<S> block() {
    expect(T::LSquare,"expected '[' to start block");lines();std::vector<S> b;
    while(!is(T::RSquare)&&!is(T::End)) {lines();if(is(T::RSquare)||is(T::End))break;b.push_back(statement());}
    expect(T::RSquare,"expected ']' to close block");return b;
  }
  S statement() {
    lines();Token t=cur();
    if(match(T::If)) {
      auto n=stmt(Stmt::IfStmt,t);n->expressions.push_back(angle());lines();n->body=block();
      lines();if(match(T::Else)) {lines();n->otherwise=block();}return n;
    }
    if(match(T::While)||match(T::Whileif)) {
      auto n=stmt(t.type==T::While?Stmt::WhileStmt:Stmt::WhileifStmt,t);
      n->expressions.push_back(angle());lines();
      if(n->kind==Stmt::WhileifStmt) {n->expressions.push_back(angle());lines();}
      n->body=block();return n;
    }
    if(match(T::Loop)) {auto n=stmt(Stmt::LoopStmt,t);lines();n->body=block();return n;}
    if(match(T::Fn)) {
      auto n=stmt(Stmt::Function,expect(T::Name,"expected function name"));
      expect(T::LParen,"expected '(' after function name");
      if(!is(T::RParen)) do {
        Token param=expect(T::Name,"expected parameter name");
        for(const auto &existing:n->params)
          if(existing==param.text)
            throw Error("SyntaxError",param.loc,"duplicate parameter '"+param.text+"'");
        n->params.push_back(param.text);
      } while(match(T::Comma));
      expect(T::RParen,"expected ')' after parameters");lines();n->body=block();return n;
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
using Value=std::variant<std::monostate,double,std::string,bool,std::shared_ptr<ArrayValue>,std::shared_ptr<Function>>;
struct ArrayValue {std::vector<Value> elements;};
struct Env:std::enable_shared_from_this<Env> {
  std::unordered_map<std::string,Value> values;std::shared_ptr<Env> parent;
  explicit Env(std::shared_ptr<Env> p=nullptr):parent(std::move(p)){}
  Value get(const std::string &s,Loc at) {
    auto it=values.find(s);if(it!=values.end())return it->second;
    if(parent)return parent->get(s,at);
    throw Error("UndefinedVariable",at,"'"+s+"' is not defined");
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
  if(auto n=std::get_if<double>(&v)) {std::ostringstream o;o.precision(15);o<<*n;return o.str();}
  if(auto s=std::get_if<std::string>(&v))return *s;
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
  if(auto n=std::get_if<double>(&v))return *n!=0;
  if(auto s=std::get_if<std::string>(&v))return !s->empty();
  if(auto b=std::get_if<bool>(&v))return *b;
  if(auto a=std::get_if<std::shared_ptr<ArrayValue>>(&v))return !(*a)->elements.empty();
  return true;
}
bool equal(const Value &a,const Value &b) {
  if(a.index()!=b.index())return false;
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
    if(auto n=std::get_if<double>(&v))return *n;
    throw Error("TypeError",at,"expected a number, got "+display(v));
  }
  size_t index(const Value &v,size_t size,Loc at) {
    auto n=std::get_if<double>(&v);
    if(!n||!std::isfinite(*n)||std::floor(*n)!=*n)
      throw Error("TypeError",at,"array index must be an integer number");
    if(*n<0||*n>=static_cast<double>(size)) {
      std::ostringstream msg;msg<<"attempted index: "<<*n<<"; valid range: ";
      if(size)msg<<"0.."<<size-1;else msg<<"empty array";
      throw Error("ArrayIndexOutOfBounds",at,msg.str());
    }
    return static_cast<size_t>(*n);
  }
  Value binary(T op,Value a,Value b,Loc at) {
    if(op==T::Equal)return equal(a,b);
    if(op==T::NotEqual)return !equal(a,b);
    if(op==T::Plus&&(std::holds_alternative<std::string>(a)||std::holds_alternative<std::string>(b)))
      return display(a)+display(b);
    double x=number(a,at),y=number(b,at);
    switch(op) {
      case T::Plus:return x+y;case T::Minus:return x-y;case T::Star:return x*y;
      case T::Slash:if(y==0)throw Error("DivisionByZero",at,"cannot divide by zero");return x/y;
      case T::Percent:if(y==0)throw Error("DivisionByZero",at,"cannot take remainder by zero");return std::fmod(x,y);
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
          try {return std::stod(e.token.text);}
          catch(const std::out_of_range &) {throw Error("NumberOutOfRange",e.token.loc,"numeric literal is too large");}
        }
        if(op==T::String)return e.token.text;
        if(op==T::True)return true;
        if(op==T::False)return false;
        return {};
      case Expr::Variable:return env->get(e.token.text,e.token.loc);
      case Expr::Array: {auto v=std::make_shared<ArrayValue>();for(auto &x:a)v->elements.push_back(evaluate(*x));return v;}
      case Expr::Unary: {Value v=evaluate(*a[0]);if(op==T::Bang)return !truth(v);
        double n=number(v,e.token.loc);return op==T::Minus?-n:n;}
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
        env->set(name,number(old,e.token.loc)+(op==T::Inc?1.0:-1.0));return old;
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
            if(auto s=std::get_if<std::string>(&args[0]))return double(s->size());
            if(auto v=std::get_if<std::shared_ptr<ArrayValue>>(&args[0]))return double((*v)->elements.size());
            throw Error("TypeError",e.token.loc,"len expects a string or array");}
        }
        Value callable=evaluate(*a[0]);auto f=std::get_if<std::shared_ptr<Function>>(&callable);
        if(!f)throw Error("TypeError",e.token.loc,"value is not callable");
        const Stmt &decl=*(*f)->declaration;
        if(args.size()!=decl.params.size())throw Error("ArityError",e.token.loc,
          "function '"+decl.token.text+"' expects "+std::to_string(decl.params.size())+
          " arguments, got "+std::to_string(args.size()));
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
int executeSource(const std::string &source,const std::string &name,Interpreter &runtime) {
  try {auto tokens=Lexer(source).scan();auto program=Parser(tokens).parse();runtime.run(std::move(program));return 0;}
  catch(const Error &e) {report(e,name,source);return 1;}
  catch(const std::exception &e) {std::cerr<<"KSL Internal Error: "<<e.what()<<"\n";return 2;}
}
} // namespace ksl
int main(int argc,char **argv) {
  if(argc>2) {std::cerr<<"Usage: ksl [file.ksl]\n";return 2;}
  if(argc==2 && std::string(argv[1])=="--help") {
    std::cout<<"KSL v0.1 — Kodexically Slow Language\n"
             <<"Usage: ksl [file.ksl]\n"
             <<"With no file, start the single-line REPL.\n";
    return 0;
  }
  if(argc==2 && std::string(argv[1])=="--version") {
    std::cout<<"KSL 0.1.0\n";
    return 0;
  }
  ksl::Interpreter runtime;
  if(argc==1) {
    std::cout<<"KSL — Kodexically Slow Language v0.1\n";
    std::string line;while(std::cout<<"> "&&std::getline(std::cin,line))
      ksl::executeSource(line+"\n","<repl>",runtime);
    return 0;
  }
  std::ifstream f(argv[1]);if(!f){std::cerr<<"KSL File Error: cannot open '"<<argv[1]<<"'\n";return 2;}
  std::ostringstream buffer;buffer<<f.rdbuf();return ksl::executeSource(buffer.str(),argv[1],runtime);
}
