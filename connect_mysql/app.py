from flask import Flask, jsonify, render_template, request, redirect, url_for, session
from flask_mysqldb import MySQL

app = Flask(__name__)
mysql = MySQL()
app.secret_key = '123456789'
app.config['MYSQL_DATABASE_USER'] = 'root'
app.config['MYSQL_DATABASE_PASSWORD'] = '' 
app.config['MYSQL_DATABASE_DB'] = 'da2'
app.config['MYSQL_DATABASE_HOST'] = 'localhost'

mysql.init_app(app)

def is_logged_in():
    return 'id' in session

@app.route("/", methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        uname = request.form['uname']
        passw = request.form['passw']
        con = mysql.connection
        cur = con.cursor()
        cur.execute("USE da2")
        cur.execute("SELECT * FROM users WHERE username = %s AND password = %s", (uname, passw))

        user = cur.fetchone()
        
        if user:
            session['id'] = user[0] 
            session['role'] = user[4]
            if session['role'] == 1:
                return redirect(url_for('admin_dashboard'))
            else:
                session['id_sp'] = user[5]
                return redirect(url_for('user_dashboard'))
        else:
            return redirect(url_for('login', error="Thông tin đăng nhập không chính xác."))

    return render_template('login.html')

@app.route("/register", methods=['GET', 'POST'])
def register():
    try:
        con = mysql.connection 
        print("Kết nối thành công!")
    except Exception as e:
        return f"Kết nối thất bại: {str(e)}"
    
    cur = con.cursor()
    cur.execute("USE da2")
    if request.method == "POST":
        name = request.form['name']
        uname = request.form['uname']
        passw = request.form['passw']
        id_sp = request.form['id_sp'] 
        
        cur.execute("SELECT * FROM id_sp WHERE id_sp = %s", (id_sp,))
        id_sp_result = cur.fetchone()

        if not id_sp_result:
            return "ID_SP không tồn tại, vui lòng kiểm tra lại!"

        cur.execute("SELECT * FROM users WHERE id_sp = %s", (id_sp,))
        sp_result = cur.fetchone()

        if sp_result:
            return "ID_SP đã được sử dụng để tạo tài khoản, không thể đăng ký."
        
        cur.execute("SELECT * FROM users WHERE username = %s", (uname,))
        user_result = cur.fetchone()

        if user_result:
            return "Tên người dùng đã tồn tại, vui lòng chọn tên khác."
        else:

            cur.execute(
                "INSERT INTO users (name, username, password, role, id_sp) VALUES (%s, %s, %s, %s, %s)",
                (name, uname, passw, 0, id_sp)
            )
            con.commit()  
            return redirect(url_for('login')) 
    return render_template('register.html')  

@app.route("/admin", methods=['GET', 'POST'])
def admin_dashboard():
    if not is_logged_in() or session['role'] != 1:
        return redirect(url_for('login')) 
    
    success_message = None
    id_sp_list = [] 

    if request.method == 'POST':
        delete_id_sp = request.form.get('delete_id_sp')
        if delete_id_sp:
            try:
                con = mysql.connection
                cur = con.cursor()
                cur.execute("USE da2")
                cur.execute("DELETE FROM sensor_nodes WHERE id_sp = %s", (delete_id_sp,))
                cur.execute("DELETE FROM id_sp WHERE id_sp = %s", (delete_id_sp,))
                con.commit()
                success_message = f"ID_SP {delete_id_sp} đã được xóa thành công!"
            except Exception as e:
                return f"Lỗi khi xóa ID_SP: {str(e)}"

        sl = request.form.get('SL')
        if sl:
            try:
                con = mysql.connection
                cur = con.cursor()
                user_id = session.get('id')
                cur.execute("USE da2")
                for _ in range(int(sl)): 
                    cur.execute("INSERT INTO id_sp (user_id) VALUES (%s)", (user_id,))
                    con.commit() 
                    cur.execute("SELECT LAST_INSERT_ID()")
                    id_sp = cur.fetchone()[0]
                    id_sp_list.append(id_sp)
                    for _ in range(2): 
                        cur.execute("INSERT INTO sensor_nodes (id_sp, temperature, humidity, fan_control, light_control, motor_control) VALUES (%s, %s, %s, %s, %s, %s)", 
                                    (id_sp, 0.0, 0.0, 0, 0, 0))

                con.commit()
                success_message = f"{sl} ID_SP đã được tạo thành công! Các ID_SP đã tạo: {', '.join(map(str, id_sp_list))}"
            except Exception as e:
                return f"Lỗi khi thêm sản phẩm: {str(e)}"

    try:
        con = mysql.connection
        cur = con.cursor()
        cur.execute("USE da2")
        cur.execute("SELECT id_sp FROM id_sp WHERE user_id = %s", (session['id'],))
        id_sp_list = [row[0] for row in cur.fetchall()] 
    except Exception as e:
        return f"Lỗi khi lấy danh sách ID_SP: {str(e)}"

    return render_template('admin.html', success_message=success_message, id_sp_list=id_sp_list)

@app.route("/user", methods=['GET', 'POST'])
def user_dashboard():
    if not is_logged_in():
        return redirect(url_for('login'))  
    id_sp = session['id_sp'] 
    con = mysql.connection
    cur = con.cursor()
    cur.execute("USE da2")

    if request.method == 'POST':
        if 'fan_control' in request.form:
            node_id = request.form['fan_control']
            cur.execute("UPDATE sensor_nodes SET fan_control = NOT fan_control, auto_control = 0 WHERE node_id = %s", (node_id,))      
        elif 'light_control' in request.form:
            node_id = request.form['light_control']
            cur.execute("UPDATE sensor_nodes SET light_control = NOT light_control, auto_control = 0 WHERE node_id = %s", (node_id,))
        elif 'motor_control' in request.form:
            node_id = request.form['motor_control']
            cur.execute("UPDATE sensor_nodes SET motor_control = NOT motor_control, auto_control = 0 WHERE node_id = %s", (node_id,))

        elif 'auto_control' in request.form:
            node_id = request.form['auto_control']
            cur.execute("UPDATE sensor_nodes SET auto_control = NOT auto_control WHERE node_id = %s", (node_id,))
        elif 'set_control' in request.form:
            node_id = request.form['set_control']
            set_temp = request.form.get(f'Set_temp_{node_id}')
            set_hum = request.form.get(f'Set_hum_{node_id}')
            cur.execute("UPDATE sensor_nodes SET set_temp = %s, set_hum = %s WHERE node_id = %s", (set_temp, set_hum, node_id))
        con.commit()
    cur.execute("SELECT * FROM sensor_nodes WHERE id_sp = %s", (id_sp,))
    sensor_nodes = cur.fetchall() 
    return render_template('user.html', sensor_nodes=sensor_nodes)

@app.route("/logout")
def logout():
    """Đăng xuất người dùng."""
    session.clear()  
    return redirect(url_for('login'))  
@app.route("/receive_data", methods=['POST'])
def receive_data():
    data = request.get_json()

    if 'id_sp' not in data or 'nodes' not in data:
        return jsonify({"error": "Dữ liệu không hợp lệ"}), 400
    id_sp = data['id_sp']
    nodes = data['nodes']
    try:
        con = mysql.connection
        cur = con.cursor()
        cur.execute("USE da2")

        for node in nodes:
            node_id = node['node_id']
            temperature = node['temperature']
            humidity = node['humidity']
            cur.execute("SELECT COUNT(*) FROM sensor_nodes WHERE id_sp = %s AND node_id = %s", (id_sp, node_id))
            count = cur.fetchone()[0]
            if count > 0:
                cur.execute("""
                    UPDATE sensor_nodes 
                    SET temperature = %s, humidity = %s 
                    WHERE id_sp = %s AND node_id = %s
                """, (temperature, humidity, id_sp, node_id))
                cur.execute("SELECT set_temp, set_hum, auto_control FROM sensor_nodes WHERE id_sp = %s AND node_id = %s", (id_sp, node_id))
                set_temp, set_hum, auto_control = cur.fetchone()
                if auto_control == 1:
                    if temperature > set_temp:
                        cur.execute("UPDATE sensor_nodes SET fan_control = 1 WHERE id_sp = %s AND node_id = %s", (id_sp, node_id))
                    else:
                        cur.execute("UPDATE sensor_nodes SET fan_control = 0 WHERE id_sp = %s AND node_id = %s", (id_sp, node_id))
                    if humidity < set_hum:
                        cur.execute("UPDATE sensor_nodes SET light_control = 1 WHERE id_sp = %s AND node_id = %s", (id_sp, node_id))
                        cur.execute("UPDATE sensor_nodes SET motor_control = 1 WHERE id_sp = %s AND node_id = %s", (id_sp, node_id))
                    else:
                        cur.execute("UPDATE sensor_nodes SET light_control = 0 WHERE id_sp = %s AND node_id = %s", (id_sp, node_id))
                        cur.execute("UPDATE sensor_nodes SET motor_control = 0 WHERE id_sp = %s AND node_id = %s", (id_sp, node_id))

            else:
                return jsonify({"error": "Node not found"}), 404

        con.commit()
        return jsonify({"message": "Cập nhật thành công"}), 200
    except Exception as e:
        print(f"Error updating database: {e}") 
        return jsonify({"error": str(e)}), 500

@app.route("/send_data", methods=['POST'])  
def send_data():
    data = request.get_json()
    if not data or 'id_sp' not in data:
        return jsonify({'error': 'id_sp not provided'}), 400
    
    id_sp = data['id_sp']  

    con = mysql.connection
    cur = con.cursor()
    cur.execute("USE da2")


    cur.execute("SELECT * FROM sensor_nodes WHERE id_sp = %s", (id_sp,))
    sensor_nodes = cur.fetchall()  

    node_list = []
    for node in sensor_nodes:
        node_data = {
            'node_id': node[0],
            'id_sp': node[1],
            'fan_control': node[4],
            'light_control': node[5],
            'motor_control': node[6]
        }
        node_list.append(node_data)

    print(node_list)
    return jsonify({'nodes': node_list})  


if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000, debug=True)
