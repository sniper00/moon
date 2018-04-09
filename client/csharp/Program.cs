using moon;
using System;
using System.Text;

class S2CLogin
{
    public string data { get; set; }
}

class C2SLogin
{
    public string data { get; set; }

    public override string ToString()
    {
        return data;
    }
}

namespace CSharpClient
{
    class Program
    {
        public static object ConvertToType(string value, Type targetType)
        {
            try
            {
                Type underlyingType = Nullable.GetUnderlyingType(targetType);

                if (underlyingType != null)
                {
                    if (string.IsNullOrEmpty(value))
                    {
                        return null;
                    }
                }
                else
                {
                    underlyingType = targetType;
                }
                switch (underlyingType.Name)
                {
                    case "S2CLogin":
                        //Console.WriteLine("recv {0}", value);
                        return new S2CLogin { data = value };
                    default:
                        return Convert.ChangeType(value, underlyingType);
                }
            }
            catch
            {
                return null;
            }
        }

        class Serializer : ISerializer
        {
            public T Deserialize<T>(byte[] bytes)
            {
                var s = Encoding.Default.GetString(bytes);
                return (T)ConvertToType(s, typeof(T));
            }

            public byte[] Serialize<TMsgID, TMsg>(TMsgID id, TMsg msg)
            {
                if (null == msg)
                {
                    return new byte[0];
                }

                return Encoding.Default.GetBytes(msg.ToString());
            }
        }

        static Network<MSGID, Serializer> net = new Network<MSGID, Serializer>();

        static async void LoginExample()
        {
            var ret = await net.Call<S2CLogin>(new C2SLogin { data = "hello" });
            //Console.WriteLine(ret.data);
            LoginExample();
        }

        static void Main(string[] args)
        {
            int serverid = net.Connect("127.0.0.1", 12345);
            net.DefaultServerID = serverid;

            LoginExample();

            while (true)
            {
                net.Update();
            }
        }
    }
}
