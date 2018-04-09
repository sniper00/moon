
using System;
using System.Collections.Generic;

namespace moon
{
    public class SessionManager
    {
        class ErrorContext
        {
            public int sessionID = 0;
            public int errorCode = 0;
            public string msg = "";
        }

        class DataContext
        {
            public int sessionID = 0;
            public byte[] data;
        }

        Dictionary<int, Session> sessions = new Dictionary<int, Session>();
        Queue<DataContext> RecvQueue = new Queue<DataContext>();
        Queue<ErrorContext> ErrorQueue = new Queue<ErrorContext>();

        public Action<int, int, string> OnError { get; set; }
        public Action<int, byte[]> OnData { get; set; }

        int IncreSessionID = 1;

        public int Connect(string ip,int port)
        {
            var ses = new Session();
            ses.OnError = PushError;
            if (ses.Connect(ip, port))
            {
                ses.OnData = PushData;

                if(IncreSessionID == int.MaxValue)
                {
                    IncreSessionID = 1;
                }

                var id = IncreSessionID ++;
                ses.sessionID = id;
                sessions.Add(id, ses);
                return id;
            }
            else
            {
                ses.Update();
            }
            return 0;
        }

        public void Update()
        {
            foreach(var s in sessions)
            {
                s.Value.Update();
            }

            lock (RecvQueue)
            {
                while (RecvQueue.Count != 0)
                {
                    var dctx = RecvQueue.Dequeue();
                    OnData(dctx.sessionID, dctx.data);
                }
            }

            lock (ErrorQueue)
            {
                if (null != OnError)
                {
                    while (ErrorQueue.Count != 0)
                    {
                        var ectx = ErrorQueue.Dequeue();
                        OnError(ectx.sessionID, ectx.errorCode, ectx.msg);
                    }
                }
            }
        }

        public void ReConnect(int sessionID)
        {
            if(sessions.ContainsKey(sessionID))
            {
                sessions[sessionID].ReConnect();
            }
        }

        public void Close(int sessionID)
        {
            if (sessions.ContainsKey(sessionID))
            {
                sessions[sessionID].Close();
            }
        }

        public void CloseAll()
        {
            foreach(var s in sessions)
            {
                s.Value.Close();
            }
        }

        public void Send(int sessionID,byte[] data)
        {
            if (sessions.ContainsKey(sessionID))
            {
                sessions[sessionID].Send(data);
            }
            else
            {
                throw new System.Exception("network send to unknown session");
            }
        }

        void PushData(int sessionID, byte[] data)
        {
            var dctx = new DataContext();
            dctx.sessionID = sessionID;
            dctx.data = data;
            lock (RecvQueue)
            {
                RecvQueue.Enqueue(dctx);
            }
        }

        void PushError(int sessionID, int errorcode, string msg)
        {
            ErrorContext ec = new ErrorContext();
            ec.sessionID = sessionID;
            ec.errorCode = errorcode;
            ec.msg = msg;

            lock (ErrorQueue)
            {
                ErrorQueue.Enqueue(ec);
            }
        }
    }
}



