/***************************************************************************
 *   Copyright (C) 2006 by TuxPLC					                                 *
 *   Author Stephane JEANNE s.jeanne@tuxplc.net                            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "LGX.h"
#include "ErrCodes.h"
#include "CIP_Const.h"
#include "CIP_IOI.h"

ReadDataService_Request *_BuildLgxReadDataRequest(char *adress,CIP_UINT number,int *requestsize)
{	
	int pathlen=_BuildIOI(NULL,adress);
	int rsize=sizeof(ReadDataService_Request)+pathlen;
	ReadDataService_Request *request=malloc(rsize);
	if (request==NULL)
	{
		CIPERROR(Sys_Error,errno,0);
		if (requestsize!=NULL) *requestsize=0;
		return(NULL);
	}
	void *Path=(void*)request+sizeof(request->Service)+sizeof(request->PathSize);
	memset(request,0,rsize);
	request->Service=CIP_DATA_READ;
	request->PathSize=pathlen/2;// size in WORD
	_BuildIOI((BYTE*)(Path),adress);
	memcpy((void*)(Path+pathlen),&number,sizeof(number));
	if (requestsize!=NULL) *requestsize=rsize;
	return(request);
}
WriteDataService_Request *_BuildLgxWriteDataRequest(char *adress,LGX_Data_Type datatype,
										void *data,int datasize,CIP_UINT number,int *requestsize)
{	
	int pathlen=_BuildIOI(NULL,adress);
	int rsize=sizeof(WriteDataService_Request)+pathlen+datasize;
	WriteDataService_Request *request=malloc(rsize);
	if (request==NULL)
	{
		CIPERROR(Sys_Error,errno,0);
		if (requestsize!=NULL) *requestsize=0;
		return(NULL);
	}
	void *Path=(void*)request+sizeof(request->Service)+sizeof(request->PathSize);
	memset(request,0,rsize);
	request->Service=CIP_DATA_WRITE;
	request->PathSize=pathlen/2;// size in WORD
	_BuildIOI((BYTE*)(Path),adress);
	memcpy((void*)(Path+pathlen),&datatype,2); // sizeof datatype is 2
	memcpy((void*)(Path+pathlen+2),&number,sizeof(number));
	memcpy((void*)(Path+pathlen+2+sizeof(number)),data,datasize);
	if (requestsize!=NULL) *requestsize=rsize;
	return(request);
}
LGX_Read *_ReadLgxData( Eip_Session *session,Eip_Connection *connection,
                            char *adress,CIP_UINT number)
{ int requestsize;
	ReadDataService_Request *request=_BuildLgxReadDataRequest(adress,number,&requestsize);
	Eip_CDI *DataItem=_ConnectedSend(session,connection,request,requestsize);
	free(request);
	if (DataItem==NULL) return(NULL);
	CommonDataService_Reply *rep=_GetService_Reply((Eip_Item*)DataItem,0);
	if (rep==NULL) return(NULL);
	if (rep->Status!=0) 
	{
		CIPERROR(AB_Error,rep->Status,_GetExtendedStatus(rep));
		free(DataItem);
		return(NULL);
	}	else
	{	
		LGX_Read *result=_DecodeLGX(rep,DataItem->Length-sizeof(DataItem->Packet));
		free(DataItem);
		return(result);
	}
}
int _WriteLgxData(Eip_Session *session,Eip_Connection *connection,
                            char *adress,LGX_Data_Type datatype,void *data,
                            /*int datasize,*/CIP_UINT number)
{ int requestsize;
	int datasize=number*_GetLGXDataSize(datatype);
	WriteDataService_Request *request=_BuildLgxWriteDataRequest(adress,datatype,data,datasize,number,&requestsize);
	Eip_CDI *DataItem=_ConnectedSend(session,connection,request,requestsize);
	free(request);
	if (DataItem==NULL) return(EX_Error);
	CommonDataService_Reply *rep=_GetService_Reply((Eip_Item*)DataItem,0);
	if (rep==NULL) return(EX_Error);
	if (rep->Status!=0) 
	{
		CIPERROR(AB_Error,rep->Status,_GetExtendedStatus(rep));
		free(DataItem);
		return(EX_Error);
	}	else
	{
		int result=_GetService_ReplyNumber((Eip_Item*)DataItem);
		free(DataItem);
		return(result);
	}
	//return(DataItem->Length);
}
int _GetService_ReplyNumber(Eip_Item *dataitem)
{ if (dataitem==NULL) {CIPERROR(Internal_Error,E_InvalidReply,0);return(0);}//E_OutOfRange
    CommonDataService_Reply *rep;
    switch (dataitem->Type_Id)
    {
			case (ItemId_UCM):rep=(CommonDataService_Reply *)((void*)dataitem+sizeof(Eip_UDI));
												break;
			case (ItemId_ConnectedTP):rep=(CommonDataService_Reply *)((void*)dataitem+sizeof(Eip_CDI));
												break;
			default:CIPERROR(Internal_Error,E_ItemUnknow,0);return(0);
    }
	switch (rep->Service)
		{
			case (CIP_MULTI_REQUEST+0x80) :
			{ 
				MultiService_Reply *temp=(MultiService_Reply*)rep;
				return(temp->Count);
			}	
			case (CIP_DATA_READ+0x80) :
			case (CIP_DATA_WRITE+0x80) :
      case (EXECUTE_PCCC+0x80) :
				return(1);
			default:CIPERROR(Internal_Error,E_UnsupportedService,0);return(0);
		}
}
CommonDataService_Reply *_GetService_Reply(Eip_Item *dataitem,unsigned int index)
{ if (dataitem==NULL) 
	{
		CIPERROR(Internal_Error,E_InvalidReply,0);
		return(NULL);
	}//E_OutOfRange
	CommonDataService_Reply *rep;
	switch (dataitem->Type_Id)
	{
		case (ItemId_UCM):rep=(CommonDataService_Reply *)((void*)dataitem+sizeof(Eip_UDI));
											break;
		case (ItemId_ConnectedTP):rep=(CommonDataService_Reply *)((void*)dataitem+sizeof(Eip_CDI));
											break;
		default:CIPERROR(Internal_Error,E_ItemUnknow,0);return(NULL);
	}
	switch (rep->Service)
	{
		case (CIP_MULTI_REQUEST+0x80) :
		{ 
			MultiService_Reply *temp=(MultiService_Reply*)rep;
			if (index+1>temp->Count) {CIPERROR(Internal_Error,E_OutOfRange,0);return(NULL);};
			return((CommonDataService_Reply *)((void *)(&temp->Count)+temp->Offsets[temp->Count]));
		}	
		case (CIP_DATA_READ+0x80) :
		case (CIP_DATA_WRITE+0x80) :
		case (EXECUTE_PCCC+0x80) :
			if (index>0) 
			{
				CIPERROR(Internal_Error,E_OutOfRange,0);
				return(NULL);
			};
			return((CommonDataService_Reply *)rep);
		default:CIPERROR(Internal_Error,E_UnsupportedService,0);return(NULL);
	}
}
LGX_Read *_DecodeLGX(CommonDataService_Reply *reply,int replysize)
{
	LGX_Read *result=malloc(sizeof(LGX_Read));
	if (result==NULL) 
	{
		CIPERROR(Sys_Error,errno,0);
		return(NULL);
	};
	memset(result,0,sizeof(LGX_Read));
	result->type=_GetLGXDataType(reply);
	result->elementsize=_GetLGXDataSize(result->type);
	void *data=_GetData(reply);
	if ((result->elementsize<=0)||(data==NULL))
	{
		CIPERROR(Internal_Error,E_LGX,__LINE__);
		free(result);
		return(NULL);
	}
	result->totalsize=replysize-((int)data-(int)reply);
	result->Varcount=result->totalsize/result->elementsize;
	if(result->Varcount<1)
	{
		CIPERROR(Internal_Error,E_LGX,__LINE__);
		free(result);
		return(NULL);
	}
	result=realloc(result,sizeof(LGX_Read)+result->totalsize);
	if (result==NULL) 
	{
		CIPERROR(Sys_Error,errno,0);
		return(NULL);
	};
	memcpy((void*)result+sizeof(LGX_Read),data,result->totalsize);
	return(result);	
	
}
LGX_Data_Type _GetLGXDataType(CommonDataService_Reply *reply)
{	if (reply==NULL) {CIPERROR(Internal_Error,E_InvalidReply,0);return(0);}
	if (reply->Status!=0) {CIPERROR(AB_Error,reply->Status,_GetExtendedStatus(reply));return(0);}
	switch (reply->Service)
	{
		case (CIP_DATA_READ+0x80):
		{	CIP_INT *type;
			type=(((void*)reply)+sizeof(CommonDataService_Reply));
			return(*type);
		}
		default:CIPERROR(Internal_Error,E_UnsupportedDataType,0);return(0);
	}
}
CIP_INT _GetExtendedStatus(CommonDataService_Reply *reply)
{	if (reply==NULL) {CIPERROR(Internal_Error,E_InvalidReply,0);return(0);}
	if (reply->Status!=0x01FF) return(0);
		else return(*((CIP_INT*)((void*)&(reply->Status)+2)));
}
float _GetLGXValueAsFloat(LGX_Read *reply,int index)
{	if ((reply->Varcount==0)||(reply->totalsize==0))
	{
		CIPERROR(Internal_Error,E_InvalidReply,0);
		return(0);
	}
	float value=0;
	CIPERROR(Internal_Error,Success,0);
	if (index>reply->Varcount-1) CIPERROR(Internal_Error,E_OutOfRange,0);
	unsigned int mask=-1;
	if (reply->mask) mask=reply->mask;
	switch (reply->type)
	{
		case DT_BOOL:value=(*((BYTE*)(((void*)reply)+sizeof(LGX_Read)+index*reply->elementsize)));break;
		case DT_BITARRAY:value=(*((CIP_DINT*)(((void*)reply)+sizeof(LGX_Read)+index*reply->elementsize)));break;
		case DT_SINT:value=(*((CIP_SINT*)(((void*)reply)+sizeof(LGX_Read)+index*reply->elementsize)));break;
		case DT_INT:value=(*((CIP_INT*)(((void*)reply)+sizeof(LGX_Read)+index*reply->elementsize)));break;
		case DT_DINT:value=(*((CIP_DINT*)(((void*)reply)+sizeof(LGX_Read)+index*reply->elementsize)));break;
		case DT_REAL:value=(*((float*)(((void*)reply)+sizeof(LGX_Read)+index*reply->elementsize)));break;
		default:CIPERROR(Internal_Error,E_UnsupportedDataType,0);return(EX_Error);
	}
	if (mask!=-1)
	{
		if (((unsigned int)value) & (mask)) return(1); else return(0);
	} else return(value);
}

int _GetLGXValueAsInteger(LGX_Read *reply,int index)
{	
	return(_GetLGXValueAsFloat(reply,index));
}
void *_GetData(CommonDataService_Reply *reply)
{	if (reply==NULL) 
	{ 
		CIPERROR(Internal_Error,E_InvalidReply,0);
		return(NULL);
	}
	if (reply->Status!=0) 
	{
		CIPERROR(AB_Error,reply->Status,_GetExtendedStatus(reply));
		return(NULL);
	}
	CIPERROR(Internal_Error,Success,0);
	switch (_GetLGXDataType(reply))
	{ 
		case LGX_BOOL:
		case LGX_BITARRAY:
		case LGX_SINT:
		case LGX_INT:
		case LGX_DINT:
		case LGX_REAL:return(((void*)reply)+sizeof(CommonDataService_Reply)+sizeof(CIP_INT));
		default:CIPERROR(Internal_Error,E_UnsupportedDataType,0);return(NULL);
	}
}
int _GetLGXDataSize(LGX_Data_Type DataType)
{
	switch (DataType)
	{ 
		case LGX_BOOL:
		case LGX_SINT:return(sizeof(CIP_SINT));
		case LGX_INT:return(sizeof(CIP_INT));
		case LGX_BITARRAY:
		case LGX_DINT:
		case LGX_REAL:return(sizeof(CIP_DINT));
		default:CIPERROR(Internal_Error,E_UnsupportedDataType,0);return(EX_Error);
	}	
}
LGX_Data_Type _LGXDataType(Data_Type DataType)
{
	switch (DataType)
	{
	case BIT:return(LGX_BOOL);break;
	case SINT:return(LGX_SINT);break;
	case TUXINT:return(LGX_INT);break; // pvbrowser modification
	//case INT:return(LGX_INT);break;
	case DINT:return(LGX_DINT);break;
	case REAL:return(LGX_REAL);break;
	default:
			return(EX_Error);
		break;
	}
}
