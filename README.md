# cpp_simple_proxy_forself

Example for php:

0. Change in your config-file address to sql server as: 
    'class' => 'yii\db\Connection',
    'dsn' => 'pgsql:host=127.0.0.1;port=54322;dbname=test_graph',
    'username' => 'postgres',
    'password' => 'hnkrwt5e', // обязателен, пустой может не сработать
    'charset' => 'utf8'
    
where "54322" - port in 127.0.0.1, which we will proxy to "5432"(standart PSQL port);

2. Run program with PORT(in code) = 54322(bind)

3. Sniff and proxy all!
