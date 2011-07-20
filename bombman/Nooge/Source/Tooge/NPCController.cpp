#include "NPCController.h"
#include "App.h"
#include "GameStage.h"
#include "RuntimeMap.h"
#include"AIMap.h"
#include<queue>
#include"FSM.h"

#include "BBombPlus.h"
#include "BDrop.h"
#include "BFaster.h"
#include "BFlamePlus.h"
#include "BPush.h"
#include "BSlower.h"
#include "BTrigger.h"

NPCController::NPCController()
{
	mDangerGrid = new AIMap(100);
	mFloodFillGrid  = new AIMap(100);
	mInterestGrid = new AIMap(0);
	mNearestBonusPos = Pos(-1,-1);//init the pos-----no bonus

	initFSM();

}

void NPCController::initFSM()
{

	//states
	flee = new FleeState(this);
	silly = new SillyState(this);
	searchBonus = new SearchBonusState(this);


	//transitions
	transToFlee = new ToFlee(this,flee);
	transToSilly = new ToSilly(this,silly);
	transToSearchBonus = new ToSearchBonus(this,searchBonus);

	//add transitions to states
	flee->AddTransition(transToFlee);//notice prority
	flee->AddTransition(transToSearchBonus);
	flee->AddTransition(transToSilly);

	silly->AddTransition(transToFlee);
	silly->AddTransition(transToSearchBonus);
	silly->AddTransition(transToSilly);

	//fsm
	mFsm = new FSM(this,silly);
	mFsm->AddState(flee);
	mFsm->AddState(searchBonus);
	mFsm->AddState(silly);

}

int NPCController::Update(Character *character, float dt)
{
	mOwner = character;
	//return rand() %4;

	//reset AIMaps
	mFloodFillGrid->Reset(100);
	mDangerGrid->Reset(100);
	mInterestGrid->Reset(0);

	computeWalls();

	computeFloodFill(character);

	computePerception(character,dt);
	return  mFsm->Update(dt);

	return 0;
}

void NPCController::computeWalls()
{
	GameStage* gs = (GameStage*)App::Inst().CurrentStage();
	//set UWALL
	GameObjectContainer* Uwall = cast<GameObjectContainer>( gs->GetChild(UWALL));
	int UwallNum = Uwall->NumOfChild();
	for(int i = 0;i<UwallNum;++i)
	{
		int row = Uwall->GetChild(i)->GetBoundingBox().Row();
		int col = Uwall->GetChild(i)->GetBoundingBox().Col();

		//set Uwall Value
		mFloodFillGrid->SetValue(col,row,-UWALL);
		mDangerGrid->SetValue(col,row,-UWALL);
		mInterestGrid->SetValue(col,row,-UWALL);
	}

	//set DWALL
	GameObjectContainer* Dwall = cast<GameObjectContainer>(gs->GetChild(DWALL));
	int DwallNum = Dwall->NumOfChild();
	for(int i = 0;i<DwallNum;++i)
	{
		int row = Dwall->GetChild(i)->GetBoundingBox().Row();
		int col = Dwall->GetChild(i)->GetBoundingBox().Col();

		//set Dwall Value
		mFloodFillGrid->SetValue(col,row,-DWALL);
		mDangerGrid->SetValue(col,row,-DWALL);
		mInterestGrid->SetValue(col,row,-DWALL);

	}
}
void NPCController::computeFloodFill(Character* character)
{
	int row = character->GetBoundingBox().Row();
	int col = character->GetBoundingBox().Col();
	//mFloodFillGrid->Reset(100);
	if(mFloodFillGrid->GetValue(col,row) == 100)//not in the wall( = =+ )
	{
		mFloodFillGrid->SetValue(col,row,0);
		computeFloodFill(col,row);
	}
}



void NPCController::computeFloodFill( int col,int row )
{
	//floodfill by queue

	static int dirX[4] = { -1, 0, 1,  0};
	static int dirY[4] = {  0, 1, 0, -1};
	std::queue<Pos> myQueue;
	myQueue.push(Pos(col,row));

	while(!myQueue.empty())
	{

		Pos pos = myQueue.front();
		myQueue.pop();
		for(int i = 0;i<4;++i)
		{
			int nextX = pos.col + dirX[i]; int nextY = pos.row + dirY[i];
			if (mFloodFillGrid->GetValue(nextX,nextY)!=-UWALL && mFloodFillGrid->GetValue(nextX,nextY)!=-DWALL && mFloodFillGrid->IsInside(nextX,nextY) )
			{
				float nextValue = mFloodFillGrid->GetValue(pos.col,pos.row) +1;
				if (nextValue< mFloodFillGrid->GetValue(nextX,nextY))
				{
					mFloodFillGrid->SetValue(nextX, nextY, nextValue);
					myQueue.push(Pos(nextX,nextY));
				}
			}
		}
	}
}

stack<Pos> NPCController::getPathTo(int col,int row)
{
	int dirX[4] = { -1, 0, 1,  0};
	int dirY[4] = {  0, 1, 0, -1};

	stack<Pos> path;
	if(mFloodFillGrid->IsInside(col,row))
	{
		float dValue = mFloodFillGrid->GetValue(col,row);
		if(dValue != 100 && dValue != -DWALL && dValue != -UWALL)
		{
			//path.push(Pos(col,row));
			int nextCol = col;
			int nextRow = row;

			while(dValue--)
			{
				path.push(Pos(nextCol,nextRow));
				for(int i = 0;i<4;++i)
				{
					if(mFloodFillGrid->GetValue(nextCol+dirX[i],nextRow+dirY[i])==dValue)
					{
						nextCol = nextCol+dirX[i];
						nextRow = nextRow + dirY[i];
						//path.push(Pos(col+dirX[i],row+dirY[i]));
						break;
					}
				}
			}
		}
	}
	return path;
}

void NPCController::computePerception(Character* character, float dt)
{
	//get bomb container
	GameStage* gs = (GameStage*)App::Inst().CurrentStage();

	computeDangerGrid(gs,character,dt);
	computeInterestGrid(gs,character,dt);


}

std::stack<Pos> NPCController::getPathTo(Pos pos)
{
	std::stack<Pos> path = getPathTo(pos.col,pos.row);
	return path;
}


void NPCController::computeDangerGrid(GameStage* gs, Character* character, float dt)
{
	const int WIDTH = mDangerGrid->GetWidth();
	const int HEIGHT = mDangerGrid->GetHeight();

	GameObjectContainer::ChildrenContainer bombs = gs->GetAllBombs();
	int nbomb = bombs.size();

	for(int t = 0;t<nbomb;++t)
	{
		Bomb* b = cast<Bomb>(gs->GetAllBombs()[t]);
		int power = b->GetPower();

		int dx[4] = {-1,0,1,0};
	    int dy[4] = {0,1,0,-1};

		for(int i = 0;i<=power;++i)
		{
			int row = b->GetBoundingBox().Row();
			int col = b->GetBoundingBox().Col();

			double remain;

			//trigger
			if(b->IsInTriggerState() && character == b->GetOwner())         //trigger bomb is mine~
			{
				continue;
			}
			else
			{
				if(b->IsInTriggerState() && character != b->GetOwner())   //trigger bomb is not mine!
					remain = 0;
				else
					remain = 3.0-b->GetTimer()->Last();   //not trigger bomb


				int dValid[4] = {1,1,1,1};
				for(int j = 0;j<4;++j)
				{
					if(dValid[j])
					{
						int nRow = row + i*dx[j];
						int nCol = col + i*dy[j];
						float rowValue = mDangerGrid->GetValue(nCol,nRow);
						if(rowValue != -UWALL)
						{
							if(rowValue!=-DWALL && remain<rowValue)
								mDangerGrid->SetValue(nCol,nRow,remain);
						}
						else
							dValid[j] = 0;
					}
				}
			}
		}
	}
}

//compute dangerous rows
/*if(row+i>=0 && row+i<=HEIGHT)
{
float rowValue = mDangerGrid->GetValue(col, row+i);

if(rowValue != -UWALL)
{
if(rowCount ==1|| rowCount ==0 )
{
rowCount =1 ;
if(rowValue !=-DWALL && remain<rowValue)
mDangerGrid->SetValue(col, row+i,remain);
}
}
else 
{
if(rowCount == 1)
rowCount = 2;
}

}

//compute dangerous cols
if(col+i>=0 && col+i<=WIDTH)
{
float colValue = mDangerGrid->GetValue(col+i,row);
if(colValue!=-DWALL)
{
if(colCount == 1 || colCount == 0)
{
colCount = 1;
if(colValue !=-DWALL && remain<colValue)
mDangerGrid->SetValue(col+i,row,remain);
}
}
else
{
if(colCount == 1)
colCount = 2;
}
}
}*/


void NPCController::computeInterestGrid(GameStage*gs,Character* character,float dt)
{

	GameObjectContainer* bonus = cast<GameObjectContainer>(gs->GetChild(BONUS));
	int nbonus = bonus->NumOfChild();
	int count = 0;
	for(int t = 0;t<nbonus;++t)
	{

		Ref<GameObject> child = bonus->GetChild(t);
		if(typeid(*child) == typeid(BFlamePlus) || typeid(*child) == typeid(BBombPlus)
			|| typeid(*child) == typeid(BFaster)
			|| typeid(*child) == typeid(BTrigger)
			|| typeid(*child) == typeid(BPush))
		{
			int col = child->GetBoundingBox().Col();
			int row = child->GetBoundingBox().Row();
			//mInterestGrid->SetValue(col,row,5);

			//get NearestBonusPos
			int value = mFloodFillGrid->GetValue(col,row);
			if(value != -DWALL && value != -UWALL && value != 100)
			{
				if(mNearestBonusPos == Pos(-1,-1))
				{
					mNearestBonusPos = Pos(col,row);
				}
				else if(value<mFloodFillGrid->GetValue(col,row))
				{
					mNearestBonusPos = Pos(col,row);
				}
			}
		}
		else if(typeid(*child)== typeid(BSlower)|| typeid(*child)== typeid(BDrop))
		{
			//mInterestGrid->SetValue(child->GetBoundingBox().Col(),child->GetBoundingBox().Row(),-5);

		}
	}
}

NPCController::~NPCController()
{
	delete mDangerGrid;
	delete mInterestGrid;
	delete mFloodFillGrid;
	delete mFsm;

	//temp
	delete flee;
	delete transToFlee;
	delete silly;
	delete transToSilly;
	delete searchBonus;
	delete transToSearchBonus;
}

Pos NPCController::NearestBonusPos()
{
	return mNearestBonusPos;
}


AIMap* NPCController::GetInterestGrid()
{
	return mInterestGrid;
}

AIMap* NPCController::GetDangerGrid()
{
	return mDangerGrid;
}

