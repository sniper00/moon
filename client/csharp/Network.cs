using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;

namespace moon
{
    public interface IHandler
    {
        void Register();
    }

    public interface ISerializer
    {
        T Deserialize<T>(byte[] data);
        byte[] Serialize<TMsgID, TMsg>(TMsgID id,TMsg msg);
    }

    class Network<TMsgID, TSerializer>
        where TSerializer: ISerializer,new()
    {
        List<IHandler> handers = new List<IHandler>();
        Dictionary<TMsgID, Action<byte[]>> MessageHanders = new Dictionary<TMsgID, Action<byte[]>>();
        Dictionary<TMsgID, TaskCompletionSource<byte[]>> ExceptHanders = new Dictionary<TMsgID, TaskCompletionSource<byte[]>>();
        Dictionary<Type, TMsgID> messageIDmap = new Dictionary<Type, TMsgID>();
        SessionManager sessionManager = new SessionManager();
        readonly TSerializer serializer = new TSerializer();
        readonly string[] msgIDNames;

        public int DefaultServerID { get; set; }

        public Action<int, int, string> OnError { set { sessionManager.OnError = value; } }

        public Action<string> OnLog;

        public Network()
        {
            msgIDNames = Enum.GetNames(typeof(TMsgID));
            sessionManager.OnData = OnData;
        }

        private void AddHandler<T>() where T : IHandler, new()
        {
            var obj = new T();
            obj.Register();
            handers.Add(obj);
        }

        public void Init()
        {
            ExceptHanders.Clear();
            if (MessageHanders.Count == 0)
            {
                //AddHandler<LoginHandler>();
                //AddHandler<GameHandler>();
                //AddHandler<MainSceneHandler>();
            }
        }

        public int Connect(string ip, int port)
        {
            return sessionManager.Connect(ip, port);
        }

        public void Update()
        {
            sessionManager.Update();
        }

        public void RegisterMessage(TMsgID id, Action<byte[]> callback)
        {
            MessageHanders[id] = callback;
        }

        TMsgID GetOrAddMessageID(Type t)
        {
            TMsgID id;
            if(!messageIDmap.TryGetValue(t, out id))
            {
                var name = t.Name;
                id = (TMsgID)Enum.Parse(typeof(TMsgID), name);
                messageIDmap.Add(t, id);
            }
            return id;
        }

        public async Task<TResponse> Call<TResponse>(object msg)
        {
            var sendmsgid = GetOrAddMessageID(msg.GetType());
            var responsemsgid = GetOrAddMessageID(typeof(TResponse));
            TaskCompletionSource<byte[]> tcs = new TaskCompletionSource<byte[]>();
            ExceptHanders.Add(responsemsgid, tcs);
            var bytes = serializer.Serialize(sendmsgid, msg);
            if (null != bytes)
            {
                sessionManager.Send(DefaultServerID, bytes);
            }
            var ret = await tcs.Task;
            return serializer.Deserialize<TResponse>(ret);
        }

        public bool Send<TMsg>(TMsg msg)
        {
            var sendmsgid = GetOrAddMessageID(msg.GetType());
            var sdata = serializer.Serialize(sendmsgid, msg);
            if (null != sdata)
            {
                sessionManager.Send(DefaultServerID, sdata);
                return true;
            }

            if(OnLog!=null)
            {
                OnLog(string.Format("Send Message {0}, Serialize error", sendmsgid));
            }
            return false;
        }

        bool Dispatch(TMsgID msgID, byte[] msgData)
        {
            if (MessageHanders.TryGetValue(msgID,out Action<byte[]> action))
            {
                action(msgData);
                return true;
            }
            return false;
        }

        void OnData(int sessionID, byte[] data)
        {
            using (MemoryStream ms = new MemoryStream(data))
            {
                using (BinaryReader br = new BinaryReader(ms))
                {
                    var msgID = (TMsgID)Enum.ToObject(typeof(TMsgID), br.ReadUInt16());
                    var msgData = br.ReadBytes((int)(ms.Length - ms.Position));
                    if (!Dispatch(msgID, msgData))
                    {
                        if (ExceptHanders.TryGetValue(msgID, out TaskCompletionSource<byte[]> tcs))
                        {
                            ExceptHanders.Remove(msgID);
                            tcs.SetResult(msgData);
                        }
                        else
                        {
                            if (OnLog != null)
                            {
                                OnLog(string.Format("message{0} not register!!", msgID));
                            }
                        }
                    }
                }
            }
        }
    }
}
