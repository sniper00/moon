using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;

namespace moon
{
    public class Session
    {
        public const int IO_BUFFER_SIZE = 1024;

        public class StateObject
        {
            public byte[] RecvBuffer = new byte[IO_BUFFER_SIZE];
            public List<byte> RecvCache = new List<byte>();
        };

        public Action<int, int, string> OnError { get; set; }
        public Action<int, byte[]> OnData { get; set; }

        Socket socket;

        Queue<byte[]> SendQueue = new Queue<byte[]>();

        string IP = "";
        int Port = 0;

        public int sessionID = 0;

        public bool Connected()
        {
            if (socket != null)
            {
                return socket.Connected;
            }
            return false;
        }

        public void Close()
        {
            try
            {
                if (socket != null)
                {
                    if (socket.Connected)
                    {
                        socket.Shutdown(SocketShutdown.Both);
                        socket.Close();
                    }
                }
            }
            catch (SocketException)
            {

            }
            catch (Exception)
            {

            }
        }

        public bool Connect(string ip, int port)
        {
            if (socket != null && socket.Connected)
            {
                Close();
            }

            try
            {
                socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                socket.Connect(ip, port);
                StateObject so = new StateObject();
                BeginReceive(so);

                IP = ip;
                Port = port;
                return true;
            }
            catch (SocketException e)
            {
                OnClose(e.ErrorCode, e.Message);
            }
            catch (Exception e)
            {
                OnClose(0, e.Message);
            }
            return false;
        }

        public void Update()
        {
            DoSend();
        }

        public bool ReConnect()
        {
            return Connect(IP, Port);
        }

        void OnClose(int errorcode, string msg)
        {
            OnError(sessionID, errorcode, msg);
        }

        void BeginReceive(StateObject so)
        {
            if (!Connected())
                return;
            socket.BeginReceive(so.RecvBuffer, 0, IO_BUFFER_SIZE, SocketFlags.None, new AsyncCallback(ReceiveCallBack), so);
        }

        void ReceiveCallBack(IAsyncResult ar)
        {
            try
            {
                StateObject so = (StateObject)ar.AsyncState;

                int length = socket.EndReceive(ar);
                if (0 == length)
                {
                    OnClose(0, "End of file");
                    return;
                }

                byte[] tmp = new byte[length];
                Buffer.BlockCopy(so.RecvBuffer, 0, tmp, 0, length);
                so.RecvCache.AddRange(tmp);

                while (so.RecvCache.Count > 2)
                {
                    MemoryStream ms = new MemoryStream(so.RecvCache.ToArray());
                    BinaryReader br = new BinaryReader(ms);
                    short size = br.ReadInt16();
                    size = IPAddress.NetworkToHostOrder((short)size);

                    //消息不完整 继续接收消息
                    if (ms.Length - ms.Position < size)
                    {
                        break;
                    }

                    var data = br.ReadBytes(size);
                    so.RecvCache.RemoveRange(0, (int)ms.Position);

                    OnData(sessionID, data);
                }

                BeginReceive(so);
            }
            catch (SocketException e)
            {
                OnClose(e.ErrorCode, e.Message);
            }
            catch (Exception e)
            {
                OnClose(0, e.Message);
            }
        }

        public void Send(byte[] data)
        {
            if (null == data || !Connected())
            {
                return;
            }

            using (MemoryStream ms = new MemoryStream())
            {
                using (BinaryWriter bw = new BinaryWriter(ms))
                {
                    var len = (short)data.Length;
                    len = IPAddress.HostToNetworkOrder(len);
                    bw.Write(len);
                    bw.Write(data);
                    SendQueue.Enqueue(ms.ToArray());
                }
            }
        }

        void DoSend()
        {
            byte[] senddata = null;

            using (MemoryStream SendBuffer = new MemoryStream(IO_BUFFER_SIZE))
            {
                using (BinaryWriter bw = new BinaryWriter(SendBuffer))
                {
                    while ((SendQueue.Count > 0) && SendBuffer.Position < IO_BUFFER_SIZE)
                    {
                        var data = SendQueue.Dequeue();
                        bw.Write(data);
                    }
                }
                senddata = SendBuffer.ToArray();
            }

            if (senddata == null || senddata.Length == 0)
                return;

            try
            {
                socket.BeginSend(senddata, 0, senddata.Length, SocketFlags.None, new AsyncCallback(SendCallBack), socket);
            }
            catch (SocketException e)
            {
                OnClose(e.ErrorCode, e.Message);
            }
            catch (Exception e)
            {
                OnClose(0, e.Message);
            }
        }

        void SendCallBack(IAsyncResult ar)
        {
            var s = (Socket)ar.AsyncState;
            s.EndSend(ar);
        }
    }
}

