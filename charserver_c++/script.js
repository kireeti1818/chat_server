const socket = new WebSocket('ws://localhost:18185');
let flag =0;
let userName;
let mode = "__broadcast___";

socket.onopen = function(event) 
{
    alert('WebSocket connection established.');
    const userNameBoxDiv = document.getElementById("userNameBox");
    userNameBoxDiv.innerHTML="<div id=\"username\"> <textarea name=\"message\"  id=\"userNameField\" \
                             rows=\"1\" cols=\"30\"></textarea><button type=\"button\" id=\"userButton\" \
                             name=\"myButton\"  onclick=\"setusername()\" >Set Username</button></div>";
};
function setusername() 
{

  const userNameField = document.getElementById("userNameField");
  userName = userNameField.value.trim();
  if (userName.length>20)
  {
    alert("user exceed limit")
    const userNameDIv = document.getElementById("error");
    userNameDIv.innerHTML = "<span style=\"color:red;\"><b><i>"+ "userName length length should be atmost 20 \
                             characters" +"<i><b><span>";
    userNameField.value="";
    return;
  }
  else{
    const userNameDIv = document.getElementById("error");
    userNameDIv.innerHTML = "";
  }
  userName=userName+'_';
  socket.send(userName);
}



socket.addEventListener('message', function (event) 
{
  if(event.data.trim()=="__false__")
  {
    console.log("userName already exists");
    alert("userName already exists");
  }
  else if(event.data.trim()=="__success__"){
      const userNameDIv = document.getElementById("username");
      userNameDIv.innerHTML = "USER NAME<br>"  +"<span style=\"font-size: 24px;\">\
                               <b><i>"+ userName +"<i><b><span>";
      const chatBox = document.getElementById("chatBox");
      chatBox.innerHTML ="<div>\
                         <input checked type=\"radio\" id=\"broadcast\" name=\"mode_type\"value=\"broadcast\">\
                         <label for=\"broadcast\" >broadcast</label>\
                         <input type=\"radio\"\id=\"private\" name=\"mode_type\" value=\"private\">\
                         <label for=\"private\">\private chat  </label>\
                         <button id=\"userButton\" type=\"button\" \onclick=\"change_mode()\">Change mode</button>\
                         <br>\
                         </div>\
                         <div id=\"userName_send\">\
                           <span style=\"color:red;\"><b> <i> broadcasting</i> the message</b></span>\
                           </div>\
                           <div id=\"main_div\">\
                           <div id=\"output\">\
                           </div>\
                           <div id=\"send\">\
                           <textarea \name=\"message\"  id=\"inputField\" rows=\"4\" cols=\"40\"></textarea>\
                           <button type=\"button\" id=\"sendButton\" name=\"myButton\"  onclick=\"sendMessage()\" >\
                           Send</button>\
                           </div>\
                           </div>"
      document.title = userName;

    }
    else if(event.data.trim()=="__usernotfound___")
    {
      alert(mode+ " User Not Found. Message not sent");
    }
    else if(event.data.trim()=="__userNameNotSet___")
    {
      alert(mode+ " user name not set. client disconnected");
      document.body.innerHTML="";
    }
    else{
      console.log('Message from server:', event.data);
      const outputDiv = document.getElementById("output");
      if(outputDiv==null)
      {
        const userNameDIv = document.getElementById("error");
        userNameDIv.innerHTML = "<span style=\"color:red;\"><b><i>"+ "userName already exists" +"<i><b><span>";
        alert("userName already exists");
      }
      else
      {
        const messageContainer = document.createElement("div");

        messageContainer.className = "recieve-message";
        messageContainer.innerHTML = event.data;
        outputDiv.appendChild(messageContainer);
      }
    }

});


function sendMessage() {
    const inputField = document.getElementById("inputField");
    const message = inputField.value.trim();
    if (message === "") {
      return; // Don't send empty messages
    }

    const outputDiv = document.getElementById("output");
    const messageContainer = document.createElement("div");

    messageContainer.className = "sent-message";
    if(mode=="__broadcast___")
    {
      messageContainer.innerHTML = "Me - " +message;
    }
    else
    {
      let str = ""+mode;
      str = str.replace(/^_+|_+$/g, '');
      messageContainer.innerHTML = "Me - " +message+ " -> "+str;

    }
    outputDiv.appendChild(messageContainer);

    inputField.value = ""; // Clear the input field after sending

    // Scroll to the bottom of the message container
    outputDiv.scrollTop = outputDiv.scrollHeight;

    // Send the message through WebSocket
    socket.send(mode+message);
  }


  function change_mode()
  {
      if(document.getElementById('private').checked) 
      {
          const userNameBoxDiv = document.getElementById("userName_send");
          userNameBoxDiv.innerHTML="<div id=\"userName_send\"><br>Reciever UserName  <textarea name=\"message\" \
                                    id=\"reciever_userName\" rows=\"1\" cols=\"20\"></textarea><button \
                                    type=\"button\" id=\"userButton\" name=\"myButton\"  \
                                    onclick=\"reciever_user()\" >send user name</button></div>";
          // document.getElementById("userName_send").innerHTML = document.getElementById("private").value + " radio button checked";  
      }
      if(document.getElementById('broadcast').checked) 
      {
          const userNameBoxDiv = document.getElementById("userName_send");
          userNameBoxDiv.innerHTML="";
          mode="__broadcast___";
          userNameBoxDiv.innerHTML= "<span style=\"color:red;\"><b> <i> broadcasting<i> the message<b><span>"


      }
  }

  function reciever_user()
  {
    const inputField = document.getElementById("reciever_userName");
    let reciever_username = inputField.value.trim();
    if (reciever_username === "") 
    {
      return; // Don't send empty messages
    }
    mode = "__"+reciever_username+"___";
    // socket.send(reciever_username1);
    const username= document.getElementById('userName_send');
    username.innerHTML= "<span style=\"color:red;\"><b>"+ "sending message to \
                         <i>"+ reciever_username +"<i><b><span>"
  }