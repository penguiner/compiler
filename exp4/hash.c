#include "hash.h"
#include <malloc.h>
#include <math.h>

unsigned int hash_pjw(char* name)
{
    unsigned int val = 0,i;
    for(; *name; ++name){
        val = (val << 2) + *name;
        if(i = val & ~0x3fff)
            val = (val ^ (i >> 12)) & 0x3fff;
    }
    return val;
}
void printinit()
{
	printf(".data\n");
	printf("_ret: .asciiz \"\\n\"\n");
	printf(".globl main\n");
	printf(".text\n\n");
	//gen write func
	printf("write:\n");
	printf("\tli $v0, 1\n");
	printf("\tsyscall\n");
	printf("\tli $v0, 4\n");
	printf("\tla $a0, _ret\n");
	printf("\tsyscall\n");
	printf("\tmove $v0, $0\n");
	printf("\tjr $ra\n");	
}

void initAllRstack()
{
	int i=9;
	while(i--)
	{
		char buf[4];
		sprintf(buf,"$t%d",i);
		Rstack[i] = MyStrCpy(buf);
	}
	RstackTop = 0;

	i=4;
	while(i--)
	{
		char buf[4];
		sprintf(buf,"$a%d",i);
		ParamRstack[i] = MyStrCpy(buf);
	}
}

void GetReg(char **reg)
{
	DEBUG printf("in GetReg %d\n",RstackTop);
	if(RstackTop==9)
		printf("run out of Reg!!!!\n");
	*reg = MyStrCpy(Rstack[RstackTop]);
	free(Rstack[RstackTop]);
	RstackTop++;
}

void FreeReg(char **reg)
{
	if(RstackTop==0)
		printf("err in FreeReg!!!\n");
	RstackTop--;
	DEBUG printf("in FreeReg %d\n",RstackTop);
	Rstack[RstackTop]=MyStrCpy(*reg);
	free(*reg);
}

void init()
{
	ID = 0;
	INIT_LIST_HEAD(&FuncListHead);
	//AddScopeNode();		//全局变量或函数变量
	int i;
	for(i = 0; i < TableSize; i++)
	{
		INIT_LIST_HEAD(&IdTable[i]);
	}
	FuncNode *FunPtr = malloc(sizeof(FuncNode));
	FunPtr->name = MyStrCpy("0");
	INIT_LIST_HEAD(&FunPtr->ScopeListHead);
	INIT_LIST_HEAD(&FunPtr->FuncList);
	list_add(&FunPtr->FuncList, &FuncListHead);
	AddScopeNode();
	assert(!list_empty(GetScopeHead()));
	initAllRstack();
	printinit();
}

void AddIdNodeList(IdNode *node)	
//将节点插入到hash表中，同时放入对应的函数作用域(链表)中
{
	DEBUG printf("in AddIdNodeList node is %s\n", node->name);
	ScopeNode *ScopePtr;
	if(node->init==1)	//FuncNode
	{
		struct list_head *ptr = FuncListHead.prev;
		FuncNode *FunPtr = list_entry(ptr, FuncNode, FuncList);
		assert(strcmp(FunPtr->name,"0")==0);
		ptr=FunPtr->ScopeListHead.next;
		ScopePtr = list_entry(ptr, ScopeNode, ScopeList);
		node->father = ScopePtr->ID;
		assert(node->father==0);
	}
	else
	{
		assert(!list_empty(GetScopeHead()));
		struct list_head *ScopeListHead = GetScopeHead();
		struct list_head *plist = ScopeListHead->next;
		ScopePtr = list_entry(plist, ScopeNode, ScopeList);
		node->father = ScopePtr->ID;
		node->init = 0;
	}
	if(ReDefine(node))		//重复定义，退出
		return;
	int val = hash_pjw(node->name);
	list_add(&node->HashList,&IdTable[val]);
	list_add_tail(&node->InSList,&ScopePtr->ScopeNodeList);
}

struct list_head *GetScopeHead()
{
	FuncNode *FunPtr = GetFirstFunNode();
	return &FunPtr->ScopeListHead;
}

void AddScopeNode()	//新加入一个作用域节点
{
	DEBUG printf("in AddScopeNode\n");
	ScopeNode *ptr = malloc(sizeof(ScopeNode));
	ptr->ID = ID++;
	INIT_LIST_HEAD(&ptr->ScopeList);
	INIT_LIST_HEAD(&ptr->ScopeNodeList);
	FuncNode *FunPtr = GetFirstFunNode();
	list_add(&ptr->ScopeList, &(FunPtr->ScopeListHead));
}

void DelScope()
{
	DEBUG printf("in DelScopeNode\n");
	assert(!list_empty(GetScopeHead()));
	FuncNode *FunPtr = GetFirstFunNode();
	struct list_head *plist = FunPtr->ScopeListHead.next;
	ScopeNode *ptr;
	ptr = list_entry(plist, ScopeNode, ScopeList);
	list_for_each(plist, &ptr->ScopeNodeList)
	{
		IdNode *p = list_entry(plist,IdNode,InSList);
		list_del(&p->InSList);
		list_del(&p->HashList);
		if(p->reg!=NULL)
		{
			if(p->reg[1]=='t')
				FreeReg(&(p->reg));
			else if(p->reg[1]='a')
				free(p->reg);
		}
		free(p);
	}
	assert(list_empty(&ptr->ScopeNodeList));
	list_del(&ptr->ScopeList);
	free(ptr);
}

FuncNode *GetFirstFunNode()
{
	assert(list_empty(&FuncListHead)==0);
	struct list_head *plist = FuncListHead.next;
	FuncNode *ptr = list_entry(plist, FuncNode, FuncList);
	return ptr;
}

bool ReDefine(IdNode *node)		//检查在同一作用域，是否重复定义
								//检查是否和函数同名
{
	DEBUG printf("in ReDefine\n");
	int val = hash_pjw(node->name);
	if(list_empty(&IdTable[val]))
		return false;
	struct list_head *plist = IdTable[val].next;
	IdNode *ptr = list_entry(plist, IdNode, HashList);
	if(strcmp(ptr->name,node->name)==0)
	{
		if(ptr->father==node->father||ptr->father==0)
		printf("ID %s redefine!!!\n",ptr->name);
		return true;
	}

	//struct list_head *plist;
	//ptr = list_entry(plist, ScopeNode, ScopeList);
	list_for_each(plist, &FuncListHead)
	{
		FuncNode *p = list_entry(plist, FuncNode, FuncList);
		if(strcmp(p->name, node->name) == 0)
			{
				printf("ID %s redefine!!!\n",node->name);
				return true;
			}
	}
	return false;
}

bool CheckInit(char *name)
{
	IdNode *ptr=CheckDefine(name);
	if(ptr==NULL)
		return false;
	else
	{
		if(ptr->init)
			return true;
		else 
		{
			//printf("Use un-init ID %s!!!\n",name);
			return false;
		}
	}
}

IdNode* CheckDefine(char *name)	//如果定义，返回该IdNode，未定义返回NULL
{
	int val = hash_pjw(name);
	if(list_empty(&IdTable[val]))
	{
		printf("Undefined ID %s!!!\n",name);
		return NULL;
	}
	struct list_head *plist = IdTable[val].next;
	IdNode *ptr = list_entry(plist, IdNode, HashList);
	assert(strcmp(ptr->name,name)==0);
	return ptr;
}

void InitIdNode(char *name,float i)
{
	int val = hash_pjw(name);
	IdNode *ptr=CheckDefine(name);
	ptr->init = 1;
	ptr->val.f = i;
}
/*

DecList     :   Dec		      {$$=$1;}               
            |   Dec COMMA DecList	{$$=$1; AddSibling($1, $3, NULL);} 	    
            ;

Dec         :   VarDec		      {$$=$1;}     //VarDec：变量或数组   
            |   VarDec ASSIGNOP Exp	{$$=$1; TreeNode *ptr = CreatNonTreeNode("Exp");AddNode(ptr,$3,NULL);AddNode($1, $2, ptr, NULL);} 	 
            ;

ExtDefList  :   ExtDef ExtDefList	{$$=$2; AddNode($$, $1, NULL); }     
            | 	        {$$=CreatNonTreeNode("DefSeq");}           
            ;
ExtDef      :   Specifier ExtDecList SEMI  {$$=CreatNonTreeNode("Def"); AddNode($$,$1, $2, NULL);}
            |   Specifier SEMI	{$$=$1;}	      
            |   Specifier FunDec CompSt	{$$=$1; AddSibling($1, $2, $3, NULL);}	
            |   error SEMI 
            ;
ExtDecList  :   VarDec		{$$=$1;}                    
            |   VarDec COMMA ExtDecList	{$$=$1; AddSibling($1,$3,NULL);}       	
            ;
VarDec      :   ID		      {$$ = $1;}         //变量       
            |   VarDec LB INT RB	{$$=CreatNonTreeNode("arrary"); AddNode($$,$1,$3, NULL);}	   //数组？？ 

*/
void analysisDefSeq(TreeNode *root)
{
	
	TreeNode *p = root;
	DEBUG printf("in analysisDefSeq %s\n",p->name);
	
	assert(strcmp(p->name,"DefSeq")==0);
	//printf("in analysisDefSeq");
	p = p->child;
	
	while(p!= NULL)
	{
		DEBUG printf("in analysisDefSeq while loop name is %s\n",p->name);
		if(strcmp(p->name,"FunNode")==0)		//定义函数
		{
			DEBUG printf("func branch\n");	
			analysisFun(p);
			p = p->sibling;
			continue;
		}
		/*
		else
		{
			if(p==NULL)		//函数体内未定义变量
				return;
			assert(p!=NULL);
			DEBUG printf("Def branch %s\n",p->name);	
			assert(strcmp(p->name,"Def")==0);
			analysisDefSeq(p->child);
			p = p->sibling;
		}
		*/
		else if(strcmp(p->name,"Def")==0)
		{
			DEBUG printf("in analysisDefSeq Def branch\n");
			//analysisDefSeq(p->child);
		DEBUG	printf("name is %s\n",p->child->name);
			assert((strcmp(p->child->name,"INT")==0)||(strcmp(p->child->name,"FLOAT")==0));
			analysisDef(p->child);
			p = p->sibling;
			//assert(p!=NULL);
		}
		else	if(strcmp(p->name,"INT")==0||strcmp(p->name,"FLOAT")==0)
		{
			analysisDef(p);
			//p = p->sibling;
			return;
		}
		else
		{
			printf("err in analysisDefSeq %s\n",p->name);
			while(1);
		}
	}
}

void analysisDef(TreeNode *root)
{
	char *name = root->name;
	int flag;	//int 0 and float 1
	if(strcmp(name,"INT")==0)
		flag = 0;
	else if (strcmp(name,"FLOAT")==0)
		flag = 1;
	else
	{	
		printf("err in analysisDef name is %s\n",name);
		while(1);
	}
	TreeNode *p = root->sibling;
	assert(p!=NULL);
	while(p!=NULL)
	{
		if(strcmp(p->name,"INT")==0||strcmp(p->name,"FLOAT")==0)
		{
			analysisDef(p);
			return;
		}
		IdNode *node = malloc(sizeof(IdNode));
		node->name=MyStrCpy(p->name);
		GetReg(&(node->reg));	
		if(flag)
			node->type=MyStrCpy("FLOAT");
		else	
			node->type=MyStrCpy("INT");
		AddIdNodeList(node);
		p = p->sibling;
	}
}

void analysisParam(TreeNode *root)
{
	char *name = root->name;
	int flag;	//int 0 and float 1
	if(strcmp(name,"INT")==0)
		flag = 0;
	else if (strcmp(name,"FLOAT")==0)
		flag = 1;
	else
	{	
		printf("err in analysisDef name is %s\n",name);
		while(1);
	}
	TreeNode *p = root->sibling;
	assert(p!=NULL);
	while(p!=NULL)
	{
		if(strcmp(p->name,"INT")==0||strcmp(p->name,"FLOAT")==0)
		{
			analysisParam(p);
			return;
		}
		IdNode *node = malloc(sizeof(IdNode));
		node->name=MyStrCpy(p->name);
		assert(ParamRegFlag<=4&&ParamRegFlag>=0);
		node->reg=MyStrCpy(ParamRstack[ParamRegFlag]);
		ParamRegFlag++;
		if(flag)
			node->type=MyStrCpy("FLOAT");
		else	
			node->type=MyStrCpy("INT");
		AddIdNodeList(node);
		p = p->sibling;

	}
}




void analysisFun(TreeNode *root)
{
	assert(strcmp(root->name,"FunNode")==0);
	FuncNode *FunPtr = malloc(sizeof(FuncNode));
	TreeNode *p = root->child;
	DEBUG printf("in analysisFun p->type %s\n",p->name);
	//get type
	FunPtr->type = MyStrCpy(p->name);
	p = p->sibling;
	DEBUG printf("IN analysisFun %s\n",p->name);
	assert(strcmp(p->name,"FunDec")==0);
	TreeNode *branch = p;
	p = p->child;
	assert(strcmp(p->type,"ID")==0);
	//get name
	FunPtr->name = MyStrCpy(p->name);
	//gen func label
	printf("%s:\n",p->name);
	IdNode *node = malloc(sizeof(IdNode));
	node->name=MyStrCpy(FunPtr->name);
	node->type=MyStrCpy(FunPtr->type);
	node->init=1;	// 函数不需要初始化	
	node->reg=MyStrCpy("$v0");
	AddIdNodeList(node);
	
	INIT_LIST_HEAD(&FunPtr->ScopeListHead);
	INIT_LIST_HEAD(&FunPtr->FuncList);
	list_add(&FunPtr->FuncList, &FuncListHead);
	p = p->sibling;
	assert(strcmp(p->name, "NewScope")==0);
	analysis(p);
	analysis(branch->sibling);
	
}

TreeNode* analysisExp(TreeNode *root)
{
	DEBUG printf("in analysisExp\n");
	TreeNode *p = root;
	if(strcmp(p->name,"Exp")==0)
	{
		DEBUG printf("analysisExp child %s\n", p->child->name);
		return analysisExp(p->child);
	}
	else if(strcmp(p->type,"ID")==0)
	{
		DEBUG printf("analysisExp ID branch %s\n", p->name);
		if(!CheckDefine(p->name))	
			return NULL;
		else 
			return	p;

	}
	else if(strcmp(p->type,"INT")==0||strcmp(p->type,"FLOAT")==0)
	{
		DEBUG printf("analysisExp number branch %s\n", p->name);
		printf("\tli $t9, %s\n",p->name);
		return p;
	}
	else if(CalcuNode(p)!=-1)	//四则运算
	{
		DEBUG printf("in CalcuNode branch type is %s %s\n",p->name,p->type);
		TreeNode *first = analysisExp(p->child);
		TreeNode *second = analysisExp(p->child->sibling);
		if(second==NULL||first==NULL)
			return NULL;
		if(strcmp(first->type,"ID")==0)
			if(!CheckInit(first->name)&&CheckDefine(first->name)->reg[1]!='a')
			{
				printf("Use un-init ID %s!!!\n",first->name);
				return NULL;
			}
		else if(strcmp(second->type,"ID")==0)
			if(!CheckInit(second->name)&&CheckDefine(second->name)->reg[1]!='a')
				{
					printf("Use un-init ID %s!!!\n",second->name);
					return NULL;
				}
		TreeNode *tmp = malloc(sizeof(TreeNode));
		assert(p->child->sibling->sibling==NULL);
		float a,b;
		a = first->val.f;
		b = second->val.f;
		DEBUG printf("first and second is %f %f\n", a,b);
		if(strcmp(first->type,"FLOAT")==0||strcmp(second->type,"FLOAT")==0)
		{
			tmp->type=MyStrCpy("FLOAT");
		}
		else
		{
			tmp->type=MyStrCpy("INT");
		}
		switch(CalcuNode(p))
		{
			case 1:
				{
					tmp->val.f = a+b;
					printf("\tadd $t9, %s, %s\t#gen add code\n",CheckDefine(first->name)->reg,CheckDefine(second->name)->reg);
					break;
				}

			case 2:
				{
					tmp->val.f = a-b; 
						printf("\tsub $t9, %s, %s\t#gen sub code\n",CheckDefine(first->name)->reg,CheckDefine(second->name)->reg);
					break;

				}
			case 3:
				{
					tmp->val.f = a*b; 
						printf("\tmul $t9, %s, %s\t#gen mult code\n",CheckDefine(first->name)->reg,CheckDefine(second->name)->reg);
					break;
				}
			case 4:
				{
					DEBUG printf("in div\n"); 
					//if(b==0){printf("Div Zero err!!!\n");break;} 
					//else 
					{
						tmp->val.f = a/b; 
							printf("\tdiv %s, %s\t#gen div code\n",CheckDefine(first->name)->reg,CheckDefine(second->name)->reg);
							printf("\tmflo $t9\n");
						break;
					}
				}
			
			default:printf("err in switch CalcuNode\n"); while(1);
		}
		return tmp;
	}
	if(strcmp(p->type,"ASSIGNOP")==0)	//赋值运算符号
	{
		DEBUG printf("in = branch\n");
		TreeNode *first = analysisExp(p->child);
		if(first==NULL)
			return NULL;
		assert(strcmp(first->type,"ID")==0);
		DEBUG printf("%s %s\n",p->child->sibling->name, p->child->sibling->type);
		TreeNode *second;
		if(p->child->sibling->type!=NULL&&strcmp(p->child->sibling->type,"ID")==0)		//func Node
		{
			second = p->child->sibling;
			GenFuncCode(second);

		}
		else
			second = analysisExp(p->child->sibling);
		if(second==NULL)
			return NULL;
		if(strcmp(CheckDefine(first->name)->type,"INT")==0&&strcmp(second->type,"FLOAT")==0)
		{
			printf("assign float number %s to int (ID %s) err!!!\n",second->name,first->name);
			return NULL;
		}
		if(strcmp(CheckDefine(first->name)->type,"INT")==0&&strcmp(second->type,"ID")==0)
		{
			if(strcmp(CheckDefine(second->name)->type,"FLOAT")==0)
			{	
				printf("assign float ID %s to int (ID %s) err!!!\n",second->name,first->name);
				return NULL;
			}
		}
		if(strcmp(second->type,"ID")==0)
		{
			//DEBUG	printf("in = branch checkinit\n");
			if(!CheckInit(second->name))
				{
					printf("Use un-init ID %s!!!\n",second->name);
					return NULL;
				}
			printf("\tmove, %s %s\n", CheckDefine(first->name)->reg, CheckDefine(second->name)->reg);
		}
		else
		{
				printf("\tmove, %s $t9\n", CheckDefine(first->name)->reg);
		}
		//DEBUG printf("first second type %s %s\n", first->type, second->type);
		InitIdNode(first->name,second->val.f);
	}
}
int CalcuNode(TreeNode *root)
{
	if(strcmp(root->type,"PLUS")==0)
		return 1;
	else if(strcmp(root->type,"MINUS")==0)
		return 2;
	else if(strcmp(root->type,"STAR")==0)
		return 3;
	else if(strcmp(root->type,"DIV")==0)
		return 4;
	else return -1;
}

void analysisReturn(TreeNode *root)
{
	TreeNode *p = root->child;
	assert(strcmp(p->name,"Exp")==0);
	TreeNode *value = analysisExp(p);
	FuncNode *FunPtr = GetFirstFunNode();
	char *type;
	if(strcmp(value->type, "ID")==0)
	{
		if(!CheckInit(value->name))
		{
			printf("return un-init ID %s!!!\n",value->name);
			return;
		}
		else
			type = CheckDefine(value->name)->type;
		//gen return code
		printf("\tmove $v0, %s\t#gen return code\n", CheckDefine(value->name)->reg);
	}
	else
		{
			type = value->type;
			printf("\tmove $v0, $t9\t#gen return code\n");

		}
	//DEBUG printf("in analysisReturn func is %s expect %s value %s\n", FunPtr->name,FunPtr->type, value->type);
	if(strcmp(FunPtr->type, "INT")==0)
		if(strcmp(type, "FLOAT")==0)
			printf ("return value err: expect %s actually get %s\n", FunPtr->type,type);
	//gen return address code
	printf("\tjr $ra\t#gen return address code\n");
}



void GenFuncCode(TreeNode *root)
{
	TreeNode *p = root->child;
	ParamRegFlag=0;
	while(p!=NULL)
	{
		assert(strcmp(p->name,"Exp")==0);
		char *reg = CheckDefine(p->child->name)->reg;
		if(reg[1]=='a')
		{
			printf("\tmove $t9, %s\t#save old ParamS\n",reg);
			free(reg);
			GetReg(&reg);
			printf("\tmove %s, $t9\n",reg);
		}
		printf("\tmove %s, %s\t#copy and rewrite ParamSReg\n",ParamRstack[ParamRegFlag++],CheckDefine(p->child->name)->reg);
		p=p->sibling;
	}
	printf("\taddi $sp, $sp, -4\t#gen general func jump code\n");
	printf("\tsw $ra, 0($sp)\n"); 
	printf("\tjal %s\n",root->name); 
	printf("\tlw $ra, 0($sp)\n");
	printf("\taddi $sp, $sp, 4\n");
}

void analysis(TreeNode *root)
{
	if(wrong)
		return;
	DEBUG	printf("in analysis name is %s\n",root->name);
	if(root==NULL)	return;
	TreeNode *p = root;
	while(p != NULL)
	{
		DEBUG printf("in analysis while loop name is %s\n",p->name);
		if(strcmp(p->name,"NewScope")==0)
		{
			AddScopeNode();
		}
		else	if(strcmp(p->name,"EndScope")==0)
		{
			DelScope();
			assert(p->child == NULL);
			assert(p->sibling==NULL);
		}
		else if (strcmp(p->name,"DefSeq")==0)
		{
			analysisDefSeq(p);		//不用分析child,analysisDefSeq已经分析
			p = p->sibling;
			//assert(strcmp(p->name,"EndScope")==0||p==NULL);
			continue;
		}
		else if(strcmp(p->name,"ParamS")==0)		//定义变量
		{
			assert(strcmp(p->child->name,"INT")==0||strcmp(p->child->name,"FLOAT")==0);
			ParamRegFlag=0;
			analysisParam(p->child);
			p = p->sibling;
			continue;
		}
		else if(strcmp(p->name,"Exp")==0)
		{
			analysisExp(p);
			p = p->sibling;
			continue;
		}
		else if(strcmp(p->name,"return")==0)
		{
			DEBUG printf("in return branch\n");			
			analysisReturn(p);
			p = p->sibling;
			continue;
		}
		else if(p->type!=NULL)
		{
			if(strcmp(p->type,"ID")==0);	//a FuncNode
			{
				GenFuncCode(p);
				p = p->sibling;
				continue;
			}

		}
		if(p->child!=NULL)
		{
			DEBUG printf("before analysis child\n");
			analysis(p->child);
		}

		p = p->sibling;
	}

}
