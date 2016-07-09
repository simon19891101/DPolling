/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013,2014 TELEMATICS LAB, DEI - Politecnico di Bari
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Giuseppe Piro <peppe@giuseppepiro.com>, <g.piro@poliba.it>
 */


#include "random-nano-routing-entity.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/packet.h"
#include "simple-nano-device.h"
#include "nano-mac-queue.h"
#include "nano-l3-header.h"
#include "nano-mac-entity.h"
#include "ns3/log.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/enum.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
#include "ns3/channel.h"
#include "simple-nano-device.h"
#include "nano-spectrum-phy.h"
#include "nano-mac-entity.h"
#include "nano-mac-header.h"
#include "ns3/seq-ts-header.h"
#include "ns3/simulator.h"
#include "nano-routing-entity.h"
#include "message-process-unit.h"
#include <stdio.h>
#include <stdlib.h>


NS_LOG_COMPONENT_DEFINE ("RandomNanoRoutingEntity");

namespace ns3 {


NS_OBJECT_ENSURE_REGISTERED (RandomNanoRoutingEntity);

TypeId RandomNanoRoutingEntity::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RandomNanoRoutingEntity")
    .SetParent<Object> ();
  return tid;
}


RandomNanoRoutingEntity::RandomNanoRoutingEntity ()
{
  SetDevice(0);
  m_receivedPacketListDim = 20;
  for (int i = 0; i < m_receivedPacketListDim; i++)
    {
	  m_receivedPacketList.push_back (9999999);
    }
  m_sentPacketListDim = 20;
  for (int i = 0; i < m_sentPacketListDim; i++)
    {
	  std::pair<uint32_t, uint32_t> item;
	  item.first = 9999999;
	  item.second = 9999999;
	  m_sentPacketList.push_back (item);
    }
}


RandomNanoRoutingEntity::~RandomNanoRoutingEntity ()
{
  SetDevice(0);
}

void 
RandomNanoRoutingEntity::DoDispose (void)
{
  SetDevice (0);
}

void
RandomNanoRoutingEntity::SendPacket (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p << "size" << p->GetSize ()<<"My ID"<<GetDevice ()->GetNode ()->GetId ()<<"Type"<<GetDevice ()->GetObject <SimpleNanoDevice> ()->m_type);

  SeqTsHeader seqTs;
  p->RemoveHeader (seqTs);
  NS_LOG_FUNCTION (this << p << "size" << p->GetSize () << seqTs);

  uint32_t l3dst = 0;
  
  std::vector<std::pair <uint32_t,uint32_t> > neighbors = GetDevice ()->GetMac ()->m_neighbors;
  uint32_t macdst = GetDevice ()->GetNode ()->GetId ();

  if (neighbors.size () != 0)
    {
	  NS_LOG_FUNCTION (this << p << "neighbors.size () > 0, try to select a nanorouter/nanointerface");

	  std::vector<std::pair <uint32_t,uint32_t> > newNeighbors;
	  std::vector<std::pair <uint32_t,uint32_t> >::iterator it;
	  for (it = neighbors.begin (); it != neighbors.end (); it++)
		{
		  uint32_t nodeType = (*it).second; //first-node ID second-node type
		  if (nodeType != 1) // it is not a nanonode 1-nanonode 2-router 3-gate see "nanospectrum"
		    {
			  NS_LOG_FUNCTION (this << "i can consider this node" << (*it).first << (*it).second);
			  newNeighbors.push_back (*it);
			}
		}
	  if (newNeighbors.size () != 0)
	    {
		  NS_LOG_FUNCTION (this << "I can choose a router/interface");
	      srand ( time(NULL) );
	      int i = rand () %newNeighbors.size();
	      macdst = newNeighbors.at (i).first;
	    }
	  else
	    {
		  NS_LOG_FUNCTION (this << "I can choose only a nanonode");
	      srand ( time(NULL) );
	      int i = rand () %neighbors.size();
	      macdst = neighbors.at (i).first;
	    }
    }

  NS_LOG_FUNCTION (this << "neighbors" << neighbors.size () << " macdst" << macdst);

  NanoL3Header header;
  uint32_t src = GetDevice ()->GetNode ()->GetId ();
  uint32_t id = seqTs.GetSeq ();
  uint32_t ttl = 100;
  header.SetSource (src);
  header.SetDestination (l3dst);
  header.SetTtl (ttl);
  header.SetPacketId (id);
  NS_LOG_FUNCTION (this << "l3 header" << header);

  p->AddHeader (seqTs);
  p->AddHeader (header);
  NS_LOG_FUNCTION (this << p << "size" << p->GetSize ());

  UpdateSentPacketId (id, macdst);

  Ptr<NanoMacEntity> mac = GetDevice ()->GetMac ();
  mac->Send (p, macdst); //invoke MAC layer
}

void
RandomNanoRoutingEntity::ReceivePacket (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p << "size" << p->GetSize ());

  SimpleNanoDevice::NodeType type = GetDevice ()->m_type; //Node type

  NanoMacHeader macHeader;
  p->RemoveHeader (macHeader);
  uint32_t from = macHeader.GetSource ();
  uint32_t to = macHeader.GetDestination ();
  NS_LOG_FUNCTION (this << macHeader);
  NS_LOG_FUNCTION (this << "my id" << GetDevice()->GetNode ()->GetId ());

  if (to == GetDevice()->GetNode ()->GetId ())
    {
	  NS_LOG_FUNCTION (this << "is for me");

	  if (GetDevice ()->GetMessageProcessUnit () && type == SimpleNanoDevice::NanoInterface)
		{
		  NS_LOG_FUNCTION (this << "I'm an interface and I have a process message unit");
		  GetDevice ()->GetMessageProcessUnit ()->ProcessMessage (p);
		}
	  else
		{
		  NS_LOG_FUNCTION (this << "forward!");
		  ForwardPacket (p, from);
		}
    }
}

void
RandomNanoRoutingEntity::ForwardPacket (Ptr<Packet> p)
{}

//note 05/08/2015
void
RandomNanoRoutingEntity::ForwardPacket (Ptr<Packet> p, uint32_t from)
{
  NS_LOG_FUNCTION (this);
  std::vector<std::pair <uint32_t,uint32_t> > neighbors = GetDevice ()->GetMac ()->m_neighbors;
  uint32_t macdst = GetDevice ()->GetNode ()->GetId ();

  NanoL3Header l3Header;
  p->RemoveHeader (l3Header);
  uint32_t id = l3Header.GetPacketId ();
  NS_LOG_FUNCTION (this << l3Header);
  NS_LOG_FUNCTION (this << "packet id" << id << "from" << from);


  uint32_t ttl = l3Header.GetTtl ();
  if (ttl > 1)
    {

	  srand ( time(NULL) );

	  if (GetDevice ()->m_type == SimpleNanoDevice::NanoRouter)//a router won't disseminate pkt to a node
		{
		  NS_LOG_FUNCTION (this << "I'm a nanorouter");
		  std::vector<std::pair <uint32_t,uint32_t> > newNeighbors;
		  std::vector<std::pair <uint32_t,uint32_t> >::iterator it;
		  for (it = neighbors.begin (); it != neighbors.end (); it++)
			{
			  uint32_t nodeId = (*it).first;
			  bool alreadySent = CheckAmongSentPacket (id, nodeId);
			  if ((*it).second != 1 && !alreadySent)
				{
				  NS_LOG_FUNCTION (this << "consider this router/interface" << (*it).first);
				  newNeighbors.push_back (*it);
				}
			}
		  NS_LOG_FUNCTION (this << "newNeighbors.size ()" << newNeighbors.size ());
		  if (newNeighbors.size () > 1)//make sure pkt won't traverse back to source
			{
			  macdst = from;
			  while (macdst == from)
			    {
				  int i = rand () %newNeighbors.size();
				  macdst = newNeighbors.at (i).first;
			    }
			}
		  else if (newNeighbors.size () == 1 && newNeighbors.at (0).first != from)
			{
			  NS_LOG_FUNCTION (this << "select the only available neighbors");
			  macdst = newNeighbors.at (0).first;
			}
		}

	  else if (GetDevice ()->m_type == SimpleNanoDevice::NanoNode)//router in newNeighbors, node in newNeighbors2
		{
		  NS_LOG_FUNCTION (this << "I'm a nanonode");
		  std::vector<std::pair <uint32_t,uint32_t> > newNeighbors;//store routers
		  std::vector<std::pair <uint32_t,uint32_t> > newNeighbors2;//store nodes
		  std::vector<std::pair <uint32_t,uint32_t> >::iterator it;
		  for (it = neighbors.begin (); it != neighbors.end (); it++)
			{
			  uint32_t nodeId = (*it).first;
			  bool alreadySent = CheckAmongSentPacket (id, nodeId);
			  if ((*it).second != 1 && !alreadySent)
				{
				  NS_LOG_FUNCTION (this << "consider this router/interface" << (*it).first);
				  newNeighbors.push_back (*it);
				}
			  else if (!alreadySent)
				{
				  NS_LOG_FUNCTION (this << "consider this nanonode" << (*it).first);
				  newNeighbors2.push_back (*it);
				}
			}
		  NS_LOG_FUNCTION (this << "newNeighbors.size ()" << newNeighbors.size ());
		  NS_LOG_FUNCTION (this << "newNeighbors2.size ()" << newNeighbors2.size ());
		  if (newNeighbors.size () > 1)//routers first
			{
			  macdst = from;
			  while (macdst == from)
				{
				  int i = rand () %newNeighbors.size();
				  macdst = newNeighbors.at (i).first;
				}
			}
		  else if (newNeighbors.size () == 1 && newNeighbors.at (0).first != from)
			{
			  NS_LOG_FUNCTION (this << "select the only available neighbors");
			  macdst = newNeighbors.at (0).first;
			}
		  else if (newNeighbors2.size () > 1)//nodes second
			{
			  macdst = from;
			  while (macdst == from)
				{
				  int i = rand () %newNeighbors2.size();
				  macdst = newNeighbors2.at (i).first;
				}
			}
		  else if (newNeighbors2.size () == 1 && newNeighbors2.at (0).first != from)
			{
			  NS_LOG_FUNCTION (this << "select the only available neighbors 2");
			  macdst = newNeighbors2.at (0).first;
			}
		}

	  NS_LOG_FUNCTION (this << "macdst" << macdst);
	  UpdateSentPacketId (id, macdst);

	  l3Header.SetTtl (ttl - 1);//reduce pkt ttl by 1
	  NS_LOG_FUNCTION (this << "new l3 header" << l3Header);
	  p->AddHeader (l3Header);
	  Ptr<NanoMacEntity> mac = GetDevice ()->GetMac ();
	  mac->Send (p, macdst);
    }
  else
    {
	  NS_LOG_FUNCTION (this << "ttl expired");
    }
}

void
RandomNanoRoutingEntity::UpdateReceivedPacketId (uint32_t id)
{
  NS_LOG_FUNCTION (this);
  m_receivedPacketList.pop_front ();
  m_receivedPacketList.push_back (id);
}

bool
RandomNanoRoutingEntity::CheckAmongReceivedPacket (uint32_t id)
{
  NS_LOG_FUNCTION (this);
  for (std::list<uint32_t>::iterator it = m_receivedPacketList.begin(); it != m_receivedPacketList.end (); it++)
    {
	  NS_LOG_FUNCTION (this << *it << id);
	  if (*it == id) return true;
    }
  return false;
}

void
RandomNanoRoutingEntity::SetReceivedPacketListDim (int m)
{
  NS_LOG_FUNCTION (this);
  m_receivedPacketListDim = m;
}


void
RandomNanoRoutingEntity::UpdateSentPacketId (uint32_t id, uint32_t nextHop)
{
  NS_LOG_FUNCTION (this << id << nextHop);
  m_sentPacketList.pop_front ();
  std::pair<uint32_t, uint32_t> item;
  item.first = id;
  item.second = nextHop;
  m_sentPacketList.push_back (item);

}

bool
RandomNanoRoutingEntity::CheckAmongSentPacket (uint32_t id, uint32_t nextHop)
{
  NS_LOG_FUNCTION (this << id << nextHop);
  for (std::list<std::pair<uint32_t, uint32_t> >::iterator it = m_sentPacketList.begin(); it != m_sentPacketList.end (); it++)
	{
	  if ((*it).first == id && (*it).second == nextHop) return true;
	}
  return false;
}

void
RandomNanoRoutingEntity::SetSentPacketListDim (int m)
{
  NS_LOG_FUNCTION (this);
  m_sentPacketListDim = m;
}

} // namespace ns3
